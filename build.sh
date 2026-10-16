#!/usr/bin/env bash
# ==============================================================================
# Build & AnyKernel3 Packaging Script for Redmi Note 12S (sea / mt6781)
# Target Kernel: Linux 6.6 LTS (Android 15 GKI 2.0)
# Compiler: Official Google Clang r510928 (LLVM 18.0.0)
# ==============================================================================

set -eo pipefail

# ------------------------------------------------------------------------------
# Colors & Formatting
# ------------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
BOLD='\033[1m'
NC='\033[0m' # No Color

info()    { echo -e "${CYAN}${BOLD}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}${BOLD}[SUCCESS]${NC} $*"; }
warn()    { echo -e "${YELLOW}${BOLD}[WARNING]${NC} $*"; }
error()   { echo -e "${RED}${BOLD}[ERROR]${NC} $*"; }
step()    { echo -e "\n${PURPLE}${BOLD}==>${WHITE} $*${NC}"; }

# ------------------------------------------------------------------------------
# Directory Setup & Configurations
# ------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="${SCRIPT_DIR}"
OUT_DIR="${KERNEL_DIR}/out"
DIST_DIR="${OUT_DIR}/dist"
MODULES_STAGING_DIR="${OUT_DIR}/modules_staging"
ANYKERNEL_DIR="${KERNEL_DIR}/tools/AnyKernel3"
ANYKERNEL_GIT="https://github.com/osm0sis/AnyKernel3.git"

DEVICE_NAME="sea"
PLATFORM="mt6781"
DEFCONFIG="sea_defconfig"
ARCH="arm64"
SUBARCH="arm64"
JOBS="$(nproc --all)"
BUILD_DATE="$(date +%Y%m%d_%H%M%S)"
ZIP_NAME="AnyKernel3-${DEVICE_NAME}-kernel6.6-${BUILD_DATE}.zip"

# Google Clang Specifications
CLANG_VERSION_TARGET="r510928"
CLANG_GIT_REPO="https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86"
CLANG_GIT_BRANCH="llvm-${CLANG_VERSION_TARGET}"

# Flags
CLEAN_BUILD=0
MENUCONFIG=0
DEFCONFIG_ONLY=0
SAVEDEFCONFIG=0
PACKAGE_ONLY=0
SKIP_MODULES=0

# ------------------------------------------------------------------------------
# CLI Arguments Parser
# ------------------------------------------------------------------------------
usage() {
    echo -e "${BOLD}Kernel 6.6 Build Script for Redmi Note 12S (sea / mt6781)${NC}

Usage: $(basename "$0") [options]

Options:
  -c, --clean           Clean output directory (out/) before building
  -m, --menuconfig      Run kernel menuconfig before compiling
  -d, --defconfig       Only generate .config from ${DEFCONFIG} and exit
  -s, --savedefconfig   Save current .config back to arch/arm64/configs/${DEFCONFIG}
  -p, --package-only    Skip compilation and package existing artifacts into AnyKernel3
      --skip-modules    Skip module compilation/staging
  -j, --jobs <N>        Number of parallel make jobs (default: all CPUs = ${JOBS})
  -h, --help            Show this help message

Environment Variables:
  CLANG_DIR             Custom path to Clang toolchain directory
  OUT_DIR               Custom output build directory (default: common/out)
  JOBS                  Override parallel threads count"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--clean)
            CLEAN_BUILD=1
            shift
            ;;
        -m|--menuconfig)
            MENUCONFIG=1
            shift
            ;;
        -d|--defconfig)
            DEFCONFIG_ONLY=1
            shift
            ;;
        -s|--savedefconfig)
            SAVEDEFCONFIG=1
            shift
            ;;
        -p|--package-only)
            PACKAGE_ONLY=1
            shift
            ;;
        --skip-modules)
            SKIP_MODULES=1
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            error "Unknown argument: $1"
            usage
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Host Dependencies Validation
# ------------------------------------------------------------------------------
check_host_dependencies() {
    step "Validating host build tools..."
    local missing=()
    local required_tools=(zip tar git)
    if [ "$PACKAGE_ONLY" -eq 0 ]; then
        required_tools+=(make bc bison flex python3)
    fi

    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing+=("$tool")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        error "Missing required build tools on host system: ${missing[*]}"
        echo ""
        echo -e "${YELLOW}Please install the missing tools using your package manager:${NC}"
        echo -e "${BOLD}Fedora / RHEL:${NC}"
        echo -e "  sudo dnf install -y make gcc clang bison flex elfutils-libelf-devel openssl-devel bc git zip tar python3 kmod\n"
        echo -e "${BOLD}Ubuntu / Debian:${NC}"
        echo -e "  sudo apt update && sudo apt install -y build-essential bc bison flex libssl-dev libelf-dev git zip tar python3 kmod\n"
        echo -e "${BOLD}Arch Linux:${NC}"
        echo -e "  sudo pacman -S --needed base-devel bc bison flex libelf openssl git zip tar python kmod\n"
        exit 1
    fi
    success "All host build tools found."
}

# ------------------------------------------------------------------------------
# Toolchain Discovery & Setup (Official Google Clang r510928)
# ------------------------------------------------------------------------------
setup_toolchain() {
    step "Setting up Google Clang toolchain (${CLANG_VERSION_TARGET})..."

    # Search candidates
    local candidates=(
        "${CLANG_DIR:-}"
        "${KERNEL_DIR}/../toolchains/clang-${CLANG_VERSION_TARGET}"
        "${KERNEL_DIR}/toolchains/clang-${CLANG_VERSION_TARGET}"
        "${HOME}/toolchains/clang-${CLANG_VERSION_TARGET}"
        "/opt/clang-${CLANG_VERSION_TARGET}"
    )

    CLANG_PATH=""
    for cand in "${candidates[@]}"; do
        if [ -n "$cand" ] && [ -f "${cand}/bin/clang" ]; then
            CLANG_PATH="$cand"
            break
        fi
    done

    # If not found, download via sparse checkout from Google
    if [ -z "$CLANG_PATH" ]; then
        local dl_target="${KERNEL_DIR}/../toolchains/clang-${CLANG_VERSION_TARGET}"
        mkdir -p "${KERNEL_DIR}/../toolchains"
        info "Clang ${CLANG_VERSION_TARGET} not found locally."
        info "Downloading official Google Clang ${CLANG_VERSION_TARGET} via sparse checkout..."
        info "Repository: ${CLANG_GIT_REPO} (branch: ${CLANG_GIT_BRANCH})"

        local tmp_tc_dir="${KERNEL_DIR}/../toolchains/.tmp_clang_${CLANG_VERSION_TARGET}"
        rm -rf "$tmp_tc_dir"
        mkdir -p "$tmp_tc_dir"

        git clone --depth=1 --filter=blob:none --sparse -b "${CLANG_GIT_BRANCH}" "${CLANG_GIT_REPO}" "$tmp_tc_dir"
        (
            cd "$tmp_tc_dir"
            git sparse-checkout set "clang-${CLANG_VERSION_TARGET}"
        )

        mv "${tmp_tc_dir}/clang-${CLANG_VERSION_TARGET}" "$dl_target"
        rm -rf "$tmp_tc_dir"
        CLANG_PATH="$dl_target"
        success "Google Clang ${CLANG_VERSION_TARGET} successfully downloaded to: ${CLANG_PATH}"
    else
        info "Found Google Clang at: ${CLANG_PATH}"
    fi

    # Export to PATH
    export PATH="${CLANG_PATH}/bin:${PATH}"

    # Verify Clang
    local clang_ver
    clang_ver="$("${CLANG_PATH}/bin/clang" --version | head -n 1)"
    info "Active Clang: ${clang_ver}"

    # Build flags
    MAKE_ARGS=(
        ARCH="${ARCH}"
        SUBARCH="${SUBARCH}"
        O="${OUT_DIR}"
        LLVM=1
        LLVM_IAS=1
        CROSS_COMPILE=aarch64-linux-gnu-
        CLANG_TRIPLE=aarch64-linux-gnu-
        -j"${JOBS}"
    )
}

# ------------------------------------------------------------------------------
# Build Kernel, DTB, DTBO and Modules
# ------------------------------------------------------------------------------
build_kernel() {
    mkdir -p "${OUT_DIR}" "${DIST_DIR}"

    if [ "$CLEAN_BUILD" -eq 1 ]; then
        step "Cleaning build directory (${OUT_DIR})..."
        make "${MAKE_ARGS[@]}" clean mrproper || true
        rm -rf "${OUT_DIR}"
        mkdir -p "${OUT_DIR}" "${DIST_DIR}"
        success "Clean completed."
    fi

    step "Configuring kernel (${DEFCONFIG})..."
    if [ ! -f "${KERNEL_DIR}/arch/${ARCH}/configs/${DEFCONFIG}" ]; then
        error "Defconfig arch/${ARCH}/configs/${DEFCONFIG} not found!"
        exit 1
    fi
    make "${MAKE_ARGS[@]}" "${DEFCONFIG}"

    if [ "$DEFCONFIG_ONLY" -eq 1 ]; then
        success "Defconfig generated successfully at: ${OUT_DIR}/.config"
        exit 0
    fi

    if [ "$MENUCONFIG" -eq 1 ]; then
        step "Launching menuconfig..."
        make "${MAKE_ARGS[@]}" menuconfig
    fi

    if [ "$SAVEDEFCONFIG" -eq 1 ]; then
        step "Saving defconfig back to arch/${ARCH}/configs/${DEFCONFIG}..."
        make "${MAKE_ARGS[@]}" savedefconfig
        cp "${OUT_DIR}/defconfig" "${KERNEL_DIR}/arch/${ARCH}/configs/${DEFCONFIG}"
        success "Updated arch/${ARCH}/configs/${DEFCONFIG}."
        exit 0
    fi

    step "Compiling Kernel Image, Device Tree Blobs & Modules..."
    info "Building Image, Image.gz, dtbs..."
    make "${MAKE_ARGS[@]}" Image Image.gz dtbs

    if [ "$SKIP_MODULES" -eq 0 ]; then
        info "Building Kernel Modules..."
        make "${MAKE_ARGS[@]}" modules
    fi

    # --------------------------------------------------------------------------
    # Collect Kernel Artifacts
    # --------------------------------------------------------------------------
    step "Collecting build artifacts..."

    # 1. Kernel Image
    local image_gz="${OUT_DIR}/arch/${ARCH}/boot/Image.gz"
    local image_raw="${OUT_DIR}/arch/${ARCH}/boot/Image"
    if [ -f "$image_gz" ]; then
        cp "$image_gz" "${DIST_DIR}/Image.gz"
        info "Kernel Image (gzip): ${DIST_DIR}/Image.gz ($(du -h "$image_gz" | cut -f1))"
    elif [ -f "$image_raw" ]; then
        cp "$image_raw" "${DIST_DIR}/Image"
        gzip -c9 "$image_raw" > "${DIST_DIR}/Image.gz"
        info "Kernel Image (raw): ${DIST_DIR}/Image ($(du -h "$image_raw" | cut -f1))"
    else
        error "Kernel Image not found after compilation!"
        exit 1
    fi

    # 2. SoC Base DTB (mt6781.dtb)
    local dtb_file="${OUT_DIR}/arch/${ARCH}/boot/dts/mediatek/${PLATFORM}.dtb"
    if [ -f "$dtb_file" ]; then
        cp "$dtb_file" "${DIST_DIR}/dtb"
        info "SoC DTB: ${DIST_DIR}/dtb ($(du -h "$dtb_file" | cut -f1))"
    else
        warn "Target DTB ${dtb_file} not found; checking mediateks..."
        local found_dtb
        found_dtb=$(find "${OUT_DIR}/arch/${ARCH}/boot/dts/mediatek/" -name "*${PLATFORM}*.dtb" | head -n 1)
        if [ -n "$found_dtb" ]; then
            cp "$found_dtb" "${DIST_DIR}/dtb"
            info "Found fallback DTB: $found_dtb -> ${DIST_DIR}/dtb"
        else
            error "No matching DTB found for ${PLATFORM}!"
            exit 1
        fi
    fi

    # 3. Board Overlay DTBO (sea.dtbo) -> dtbo.img
    local dtbo_file="${OUT_DIR}/arch/${ARCH}/boot/dts/mediatek/${DEVICE_NAME}.dtbo"
    local mkdtboimg="${KERNEL_DIR}/scripts/mkdtboimg.py"

    if [ ! -f "$mkdtboimg" ]; then
        error "mkdtboimg.py not found at: ${mkdtboimg}!"
        exit 1
    fi

    if [ -f "$dtbo_file" ]; then
        info "Generating dtbo.img from ${dtbo_file} using mkdtboimg.py (page size: 2048)..."
        python3 "$mkdtboimg" create "${DIST_DIR}/dtbo.img" --page_size=2048 "$dtbo_file"
        info "DTBO Image: ${DIST_DIR}/dtbo.img ($(du -h "${DIST_DIR}/dtbo.img" | cut -f1))"
    else
        warn "Board DTBO ${dtbo_file} not found; checking fallback overlays..."
        local found_dtbo
        found_dtbo=$(find "${OUT_DIR}/arch/${ARCH}/boot/dts/mediatek/" -name "*${DEVICE_NAME}*.dtbo" | head -n 1)
        if [ -n "$found_dtbo" ]; then
            python3 "$mkdtboimg" create "${DIST_DIR}/dtbo.img" --page_size=2048 "$found_dtbo"
            info "DTBO Image generated from: ${found_dtbo}"
        else
            error "No DTBO overlay found for ${DEVICE_NAME}!"
            exit 1
        fi
    fi

    # 4. Modules Staging
    if [ "$SKIP_MODULES" -eq 0 ]; then
        step "Staging & stripping vendor kernel modules..."
        rm -rf "${MODULES_STAGING_DIR}"
        mkdir -p "${MODULES_STAGING_DIR}" "${DIST_DIR}/modules/vendor/lib/modules"

        make "${MAKE_ARGS[@]}" INSTALL_MOD_PATH="${MODULES_STAGING_DIR}" INSTALL_MOD_STRIP=1 modules_install

        local mod_count=0
        while IFS= read -r mod; do
            local mod_name
            mod_name="$(basename "$mod")"
            cp -f "$mod" "${DIST_DIR}/modules/vendor/lib/modules/${mod_name}"
            ((mod_count++))
        done < <(find "${MODULES_STAGING_DIR}" -name "*.ko" -type f)

        success "Collected and stripped ${mod_count} kernel modules into: ${DIST_DIR}/modules/vendor/lib/modules/"
    fi
}

# ------------------------------------------------------------------------------
# AnyKernel3 Setup & Packaging
# ------------------------------------------------------------------------------
package_anykernel() {
    step "Packaging AnyKernel3 flashable zip..."
    mkdir -p "${DIST_DIR}"

    # Ensure AnyKernel3 template is present
    if [ ! -d "${ANYKERNEL_DIR}" ] || [ ! -f "${ANYKERNEL_DIR}/anykernel.sh" ]; then
        info "Cloning AnyKernel3 template into: ${ANYKERNEL_DIR}..."
        mkdir -p "$(dirname "${ANYKERNEL_DIR}")"
        rm -rf "${ANYKERNEL_DIR}"
        git clone --depth=1 "${ANYKERNEL_GIT}" "${ANYKERNEL_DIR}"
    fi

    # Prepare staging directory
    local ak_staging="${OUT_DIR}/anykernel_staging"
    rm -rf "$ak_staging"
    mkdir -p "$ak_staging"
    cp -r "${ANYKERNEL_DIR}/"* "$ak_staging/"
    rm -rf "$ak_staging/.git" "$ak_staging/.github" "$ak_staging/README.md"

    # Configure anykernel.sh for Redmi Note 12S (sea / mt6781)
    cat << 'EOF' > "$ak_staging/anykernel.sh"
### AnyKernel3 Ramdisk Mod Script
## Redmi Note 12S (sea / mt6781) - Linux 6.6 GKI 2.0

### AnyKernel setup
# global properties
properties() { '
kernel.string=Kernel 6.6 LTS (GKI 2.0) for Redmi Note 12S (sea)
do.devicecheck=1
do.modules=1
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=sea
device.name2=mt6781
device.name3=23030RAC7Y
device.name4=2303ERA42L
device.name5=
supported.versions=13-16
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=auto;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# boot install
dump_boot;
write_boot;
## end boot install
EOF
    chmod +x "$ak_staging/anykernel.sh"

    # Copy Kernel Image
    if [ -f "${DIST_DIR}/Image.gz" ]; then
        cp -f "${DIST_DIR}/Image.gz" "$ak_staging/Image.gz"
    elif [ -f "${DIST_DIR}/Image" ]; then
        cp -f "${DIST_DIR}/Image" "$ak_staging/Image"
    elif [ -f "${KERNEL_DIR}/../modules-6.6/kernel" ]; then
        info "Using reference 6.6 kernel from modules-6.6/kernel..."
        cp -f "${KERNEL_DIR}/../modules-6.6/kernel" "$ak_staging/Image.gz"
    else
        error "No Kernel Image found in ${DIST_DIR} to package!"
        exit 1
    fi

    # Copy DTB
    if [ -f "${DIST_DIR}/dtb" ]; then
        cp -f "${DIST_DIR}/dtb" "$ak_staging/dtb"
    elif [ -f "${KERNEL_DIR}/../modules-6.6/dtb.img" ]; then
        info "Using reference 6.6 dtb from modules-6.6/dtb.img..."
        cp -f "${KERNEL_DIR}/../modules-6.6/dtb.img" "$ak_staging/dtb"
    else
        error "No dtb found in ${DIST_DIR} to package!"
        exit 1
    fi

    # Copy DTBO
    if [ -f "${DIST_DIR}/dtbo.img" ]; then
        cp -f "${DIST_DIR}/dtbo.img" "$ak_staging/dtbo.img"
    elif [ -f "${KERNEL_DIR}/../modules-6.6/dtbo.img" ]; then
        info "Using reference 6.6 dtbo from modules-6.6/dtbo.img..."
        cp -f "${KERNEL_DIR}/../modules-6.6/dtbo.img" "$ak_staging/dtbo.img"
    else
        error "No dtbo.img found in ${DIST_DIR} to package!"
        exit 1
    fi

    # Copy Modules
    mkdir -p "$ak_staging/modules/vendor/lib/modules"
    if [ -d "${DIST_DIR}/modules/vendor/lib/modules" ] && [ "$(ls -A "${DIST_DIR}/modules/vendor/lib/modules" 2>/dev/null)" ]; then
        cp -rf "${DIST_DIR}/modules/vendor/lib/modules/"* "$ak_staging/modules/vendor/lib/modules/"
    elif [ -d "${KERNEL_DIR}/../modules-6.6/vendor_dlkm" ]; then
        info "Staging reference modules from modules-6.6/vendor_dlkm..."
        find "${KERNEL_DIR}/../modules-6.6/vendor_dlkm" -name "*.ko" -exec cp -f {} "$ak_staging/modules/vendor/lib/modules/" \;
    fi

    # Create Zip Package
    local final_zip="${DIST_DIR}/${ZIP_NAME}"
    (
        cd "$ak_staging"
        zip -r9 -q "${final_zip}" * -x "placeholder"
    )

    # Compute SHA256 checksum
    local sha256_val
    sha256_val="$(sha256sum "${final_zip}" | awk '{print $1}')"
    echo "${sha256_val}  $(basename "${final_zip}")" > "${final_zip}.sha256"

    # Cleanup staging
    rm -rf "$ak_staging"

    # --------------------------------------------------------------------------
    # Final Summary Banner
    # --------------------------------------------------------------------------
    echo -e "\n${GREEN}${BOLD}========================================================================${NC}"
    echo -e "${GREEN}${BOLD}                BUILD & PACKAGING COMPLETED SUCCESSFULLY!              ${NC}"
    echo -e "${GREEN}${BOLD}========================================================================${NC}"
    echo -e " ${BOLD}Device:${NC}                Redmi Note 12S (${DEVICE_NAME} / ${PLATFORM})"
    echo -e " ${BOLD}Kernel Version:${NC}        Linux 6.6 LTS (Android 15 GKI 2.0)"
    echo -e " ${BOLD}Compiler:${NC}              Google Clang (${CLANG_VERSION_TARGET})"
    echo -e " ${BOLD}Flashable Zip:${NC}         ${WHITE}${final_zip}${NC}"
    echo -e " ${BOLD}Zip File Size:${NC}         $(du -h "${final_zip}" | cut -f1)"
    echo -e " ${BOLD}SHA256 Checksum:${NC}       ${CYAN}${sha256_val}${NC}"
    echo -e " ${BOLD}Included Components:${NC}"
    echo -e "   - Kernel:            Image.gz"
    echo -e "   - SoC Device Tree:   dtb (${PLATFORM}.dtb)"
    echo -e "   - Board Overlay:     dtbo.img (${DEVICE_NAME}.dtbo)"
    echo -e "   - Modules Staging:   modules/vendor/lib/modules/ (.ko)"
    echo -e "${GREEN}${BOLD}========================================================================${NC}"
    echo -e " ${YELLOW}How to Flash:${NC}"
    echo -e "   1. Boot device into Recovery (TWRP / OrangeFox / PBRP)"
    echo -e "   2. Sideload or Install Zip: ${WHITE}adb sideload ${final_zip}${NC}"
    echo -e "   3. Reboot System & Enjoy!"
    echo -e "${GREEN}${BOLD}========================================================================${NC}\n"
}

# ------------------------------------------------------------------------------
# Main Flow
# ------------------------------------------------------------------------------
main() {
    local start_time
    start_time="$(date +%s)"

    check_host_dependencies

    if [ "$PACKAGE_ONLY" -eq 0 ]; then
        setup_toolchain
        build_kernel
    else
        info "Skipping compilation (--package-only flag active). Packaging existing artifacts..."
    fi

    package_anykernel

    local end_time
    end_time="$(date +%s)"
    local duration=$((end_time - start_time))
    info "Total time elapsed: $((duration / 60)) min $((duration % 60)) sec."
}

main "$@"
