// SPDX-License-Identifier: GPL-2.0
//
// mt6781-mt6366.c  --  mt6781 mt6366 ALSA SoC machine driver
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Patrick Cheng <Cheng-yi.Cheng@mediatek.com>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-afe-platform-driver.h"
#include "mt6781-afe-common.h"
#include "mt6781-afe-clk.h"
#include "mt6781-afe-gpio.h"
#include "../../codecs/mt6358.h"
#include "../common/mtk-sp-spk-amp.h"

#if defined(CONFIG_SND_SOC_FS18XX)
#include "../../codecs/fs1815/fsm_public.h"
#endif

#if defined(CONFIG_SND_SOC_SIA81XX)
#include "../../codecs/sia81xx/sia81xx_aux_dev_if.h"
#endif

#if defined(CONFIG_SND_SOC_AW87XXX)
char* mode_function[] = { "Off", "Receiver", "Fm", "Voice", "Music" };
extern int aw87xxx_set_profile(int, char *);
enum aw87xxx_scene_mode {
	AW87XXX_OFF_MODE = 0,
	AW87XXX_RCV_MODE = 1,
	AW87XXX_FM_MODE = 2,
	AW87XXX_VOICE_MODE = 3,
	AW87XXX_MUSIC_MODE = 4,
	AW87XXX_MODE_MAX = 5,
};

enum aw87xxx_channel {
	AW87XXX_LEFT_CHANNEL = 0,
	AW87XXX_RIGHT_CHANNEL = 1,
};
#endif

/*
 * if need additional control for the ext spk amp that is connected
 * after Lineout Buffer / HP Buffer on the codec, put the control in
 * mt6781_mt6366_spk_amp_event()
 */

// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
#define EXT_RCV_AMP_W_NAME "Ext_Reciver_Amp"
#endif

#define EXT_SPK_AMP_W_NAME "Ext_Speaker_Amp"

static const char *const mt6781_spk_type_str[] = {MTK_SPK_NOT_SMARTPA_STR,
						  MTK_SPK_RICHTEK_RT5509_STR,
						  MTK_SPK_MEDIATEK_MT6660_STR};
static const char *const mt6781_spk_i2s_type_str[] = {MTK_SPK_I2S_0_STR,
						      MTK_SPK_I2S_1_STR,
						      MTK_SPK_I2S_2_STR,
						      MTK_SPK_I2S_3_STR,
						      MTK_SPK_I2S_5_STR};

static const struct soc_enum mt6781_spk_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6781_spk_type_str),
			    mt6781_spk_type_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6781_spk_i2s_type_str),
			    mt6781_spk_i2s_type_str),
};

static int mt6781_spk_type_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6781_spk_i2s_out_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_out_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6781_spk_i2s_in_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_in_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
static int rcv_amp_mode;
static const char *rcv_amp_type_str[] = {"SPEAKER_MODE", "RECIEVER_MODE", "FM_MODE", "VOICE_MODE"};
static const struct soc_enum rcv_amp_type_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rcv_amp_type_str), rcv_amp_type_str);

static int mt6781_rcv_amp_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s() = %d\n", __func__, rcv_amp_mode);
	ucontrol->value.integer.value[0] = rcv_amp_mode;
	return 0;
}

static int mt6781_rcv_amp_mode_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	rcv_amp_mode = ucontrol->value.integer.value[0];
	pr_info("%s() = %d\n", __func__, rcv_amp_mode);
	return 0;
}

static int spk_amp_mode;
static const char *spk_amp_type_str[] = {"SPEAKER_MODE", "RECIEVER_MODE", "FM_MODE", "VOICE_MODE"};
static const struct soc_enum spk_amp_type_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_amp_type_str), spk_amp_type_str);

static int mt6781_spk_amp_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s() = %d\n", __func__, spk_amp_mode);
	ucontrol->value.integer.value[0] = spk_amp_mode;
	return 0;
}

static int mt6781_spk_amp_mode_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	spk_amp_mode = ucontrol->value.integer.value[0];
	pr_info("%s() = %d\n", __func__, spk_amp_mode);
	return 0;
}
#endif
// ALPS05007528 end

static int mt6781_mt6366_spk_amp_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
		if (1 == spk_amp_mode) {
			dev_info(card->dev, "%s(), spk_amp_audio_krcv(), mode = %d\n", __func__, spk_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(15);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[spk_amp_mode]);
			#endif
		} else if (2 == spk_amp_mode) {
			dev_info(card->dev, "%s(), spk_amp_audio_kfm() mode = %d\n", __func__, spk_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(0);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[spk_amp_mode]);
			#endif
		} else if (3 == spk_amp_mode) {
			dev_info(card->dev, "%s(), spk_amp_audio_kvoice() mode = %d\n", __func__, spk_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(1);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[spk_amp_mode]);
			#endif
		} else {
			dev_info(card->dev, "%s(), spk_amp_audio_kspk() mode = %d\n", __func__, spk_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(0);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[AW87XXX_MUSIC_MODE]);
			#endif
		}
#endif
// ALPS05007528 end
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
		dev_info(card->dev, "%s(), spk_amp_audio_off()\n", __func__);
		#if defined(CONFIG_SND_SOC_FS18XX)
		fsm_speaker_off();
		#endif
		#if defined(CONFIG_SND_SOC_AW87XXX)
		/*add for HTH-284975 by lxd at 2023/3/7 start */
		aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[AW87XXX_OFF_MODE]);
		/*add for HTH-284975 by lxd at 2023/3/7 end */
		aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[AW87XXX_OFF_MODE]);
		#endif
#endif
// ALPS05007528 end
		break;
	default:
		break;
	}

	return 0;
};

// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
static int mt6781_mt6366_rcv_amp_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
		if (1 == rcv_amp_mode) {
			dev_info(card->dev, "%s(), rcv_amp_audio_drcv() mode = %d\n", __func__, rcv_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(15);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[rcv_amp_mode]);
			#endif
		} else if (2 == rcv_amp_mode) {
			dev_info(card->dev, "%s(), rcv_amp_audio_dfm() mode = %d\n", __func__, rcv_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(0);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[rcv_amp_mode]);
			#endif
		} else if (3 == spk_amp_mode) {
			dev_info(card->dev, "%s(), rcv_amp_audio_kvoice() mode = %d\n", __func__, rcv_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(1);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[rcv_amp_mode]);
			#endif
		} else {
			dev_info(card->dev, "%s(), rcv_amp_audio_dspk() mode = %d\n", __func__, rcv_amp_mode);
			#if defined(CONFIG_SND_SOC_FS18XX)
			fsm_set_scene(0);
			fsm_speaker_onn();
			#endif
			#if defined(CONFIG_SND_SOC_AW87XXX)
			aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[AW87XXX_MUSIC_MODE]);
			#endif
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
		dev_info(card->dev, "%s(), rcv_amp_audio_off()\n", __func__);
		#if defined(CONFIG_SND_SOC_FS18XX)
		fsm_speaker_off();
		#endif
		#if defined(CONFIG_SND_SOC_AW87XXX)
		aw87xxx_set_profile(AW87XXX_LEFT_CHANNEL, mode_function[AW87XXX_OFF_MODE]);
		/*add for HTH-284975 by lxd at 2023/3/7 start */
		aw87xxx_set_profile(AW87XXX_RIGHT_CHANNEL, mode_function[AW87XXX_OFF_MODE]);
		/*add for HTH-284975 by lxd at 2023/3/7 end */
		#endif
		break;
	default:
		break;
	}

	return 0;
};
#endif
// ALPS05007528 end

#ifdef CONFIG_SND_SOC_MT8185_EVB
static int mt6781_mt6366_headphone_amp_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
#ifdef CONFIG_SND_SOC_MT8185_EVB
		audio_exthpamp_enable();
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
#ifdef CONFIG_SND_SOC_MT8185_EVB
		audio_exthpamp_disable();
#endif
		break;
	default:
		break;
	}

	return 0;
};
#endif

static const struct snd_soc_dapm_widget mt6781_mt6366_widgets[] = {
	SND_SOC_DAPM_SPK(EXT_SPK_AMP_W_NAME, mt6781_mt6366_spk_amp_event),
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
	SND_SOC_DAPM_SPK(EXT_RCV_AMP_W_NAME, mt6781_mt6366_rcv_amp_event),
#endif
// ALPS05007528 end
};

static const struct snd_soc_dapm_route mt6781_mt6366_routes[] = {
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L"},
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L HSSPK"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone L Ext Spk Amp"},
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
	{EXT_RCV_AMP_W_NAME, NULL, "Receiver"},
	{EXT_RCV_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
#else
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
#endif
// ALPS05007528 end
};

static const struct snd_kcontrol_new mt6781_mt6366_controls[] = {
	SOC_DAPM_PIN_SWITCH(EXT_SPK_AMP_W_NAME),
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
	SOC_DAPM_PIN_SWITCH(EXT_RCV_AMP_W_NAME),
	SOC_ENUM_EXT("RCV_AMP_MODE", rcv_amp_type_enum,
		     mt6781_rcv_amp_mode_get, mt6781_rcv_amp_mode_set),
	SOC_ENUM_EXT("SPK_AMP_MODE", spk_amp_type_enum,
		     mt6781_spk_amp_mode_get, mt6781_spk_amp_mode_set),
#endif
// ALPS05007528 end
	SOC_ENUM_EXT("MTK_SPK_TYPE_GET", mt6781_spk_type_enum[0],
		     mt6781_spk_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_OUT_TYPE_GET", mt6781_spk_type_enum[1],
		     mt6781_spk_i2s_out_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_IN_TYPE_GET", mt6781_spk_type_enum[1],
		     mt6781_spk_i2s_in_type_get, NULL),
};

/*
 * define mtk_spk_i2s_mck node in dts when need mclk,
 * BE i2s need assign snd_soc_ops = mt6781_mt6366_i2s_ops
 */
static int mt6781_mt6366_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;

	return snd_soc_dai_set_sysclk(asoc_rtd_to_cpu(rtd, 0),
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt6781_mt6366_i2s_ops = {
	.hw_params = mt6781_mt6366_i2s_hw_params,
};

static int mt6781_mt6366_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6781_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		asoc_rtd_to_codec(rtd, 0)->component;
	int phase = 0;
	unsigned int monitor = 0;
	int test_done_1, test_done_2 = 0;
	int miso0_need_calib, miso1_need_calib;
	int cycle_1, cycle_2, prev_cycle_1, prev_cycle_2 = 0;
	int counter = 0;

	dev_info(afe->dev, "%s(), start\n", __func__);

	miso0_need_calib = mt6781_afe_gpio_is_prepared(MT6781_AFE_GPIO_DAT_MISO0_ON);
	miso1_need_calib = mt6781_afe_gpio_is_prepared(MT6781_AFE_GPIO_DAT_MISO1_ON);

	pm_runtime_get_sync(afe->dev);
	mt6781_afe_gpio_request(afe, true, MT6781_DAI_ADDA, 1);
	mt6781_afe_gpio_request(afe, true, MT6781_DAI_ADDA, 0);

	mt6358_mtkaif_calibration_enable(codec_component);

	/* set clock protocol 2 */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x39);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
			   0xffff, 0x4);

	afe_priv->mtkaif_calibration_num_phase = RG_AUD_PAD_TOP_PHASE_MODE_MASK;
	afe_priv->mtkaif_calibration_ok = true;
	afe_priv->mtkaif_chosen_phase[0] = -1;
	afe_priv->mtkaif_chosen_phase[1] = -1;

	for (phase = 0;
	     phase <= afe_priv->mtkaif_calibration_num_phase &&
	     afe_priv->mtkaif_calibration_ok;
	     phase++) {
		mt6358_set_mtkaif_calibration_phase(codec_component,
						    phase, phase);

		regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
				   0x1, 0x1);

		test_done_1 = miso0_need_calib ? 0 : -1;
		test_done_2 = miso1_need_calib ? 0 : -1;
		cycle_1 = -1;
		cycle_2 = -1;
		counter = 0;
		while (test_done_1 == 0 || test_done_2 == 0) {
			regmap_read(afe_priv->topckgen, CKSYS_AUD_TOP_MON,
				    &monitor);
			/* get test status */
			if (test_done_1 == 0)
				test_done_1 = (monitor >> 28) & 0x1;
			if (test_done_2 == 0)
				test_done_2 = (monitor >> 29) & 0x1;

			/* get delay cycle */
			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;

			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_err(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					__func__,
					cycle_1, cycle_2, monitor);
				afe_priv->mtkaif_calibration_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (miso0_need_calib && cycle_1 != prev_cycle_1 &&
		    afe_priv->mtkaif_chosen_phase[0] < 0) {
			afe_priv->mtkaif_chosen_phase[0] = phase - 1;
			afe_priv->mtkaif_phase_cycle[0] = prev_cycle_1;
		}

		if (miso1_need_calib && cycle_2 != prev_cycle_2 &&
		    afe_priv->mtkaif_chosen_phase[1] < 0) {
			afe_priv->mtkaif_chosen_phase[1] = phase - 1;
			afe_priv->mtkaif_phase_cycle[1] = prev_cycle_2;
		}

		regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
				   0x1, 0x0);


	}

	mt6358_set_mtkaif_calibration_phase(codec_component,
			(afe_priv->mtkaif_chosen_phase[0] < 0) ?
			0 : afe_priv->mtkaif_chosen_phase[0],
			(afe_priv->mtkaif_chosen_phase[1] < 0) ?
			0 : afe_priv->mtkaif_chosen_phase[1]);

	/* disable rx fifo */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);

	mt6358_mtkaif_calibration_disable(codec_component);

	mt6781_afe_gpio_request(afe, false, MT6781_DAI_ADDA, 1);
	mt6781_afe_gpio_request(afe, false, MT6781_DAI_ADDA, 0);

	/* disable syncword if miso pin not prepared */
	if (!miso0_need_calib)
		regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG,
				   RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK_SFT,
				   0x1 << RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_SFT);
	if (!miso1_need_calib)
		regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG,
				   RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK_SFT,
				   0x1 << RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT);
	pm_runtime_put(afe->dev);

	dev_info(afe->dev, "%s(), mtkaif_chosen_phase[0/1]:%d/%d, miso_need_calib[%d/%d]\n",
		 __func__,
		 afe_priv->mtkaif_chosen_phase[0],
		 afe_priv->mtkaif_chosen_phase[1],
		 miso0_need_calib, miso1_need_calib);

	return 0;
}

static int mt6781_mt6366_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6781_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dapm_context *dapm = &rtd->card->dapm;
	struct snd_soc_component *codec_component =
		asoc_rtd_to_codec(rtd, 0)->component;

	/* set mtkaif protocol */
	mt6358_set_mtkaif_protocol(codec_component,
				   MT6358_MTKAIF_PROTOCOL_1);
	afe_priv->mtkaif_protocol = MT6358_MTKAIF_PROTOCOL_1;

	/* mtkaif calibration */
	if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2_CLK_P2)
		mt6781_mt6366_mtkaif_calibration(rtd);

	/* disable ext amp connection */
	snd_soc_dapm_disable_pin(dapm, EXT_SPK_AMP_W_NAME);
// ALPS05007528 begin
#if defined(CONFIG_SND_SOC_DSPK_LOL_HP)
	snd_soc_dapm_disable_pin(dapm, EXT_RCV_AMP_W_NAME);
#endif
// ALPS05007528 end

	return 0;
}

static int mt6781_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	dev_info(rtd->dev, "%s(), fix format to 32bit\n", __func__);

	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static const struct snd_pcm_hardware mt6781_mt6366_vow_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 2 * 1024,
	.periods_min = 2,
	.periods_max = 4,
	.buffer_bytes_max = 2 * 2 * 1024,
};

static int mt6781_mt6366_vow_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *comp = NULL;
	struct snd_soc_rtdcom_list *rtdcom = NULL;

	dev_info(afe->dev, "%s(), start\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &mt6781_mt6366_vow_hardware);

	mt6781_afe_gpio_request(afe, true, MT6781_DAI_VOW, 0);

	/* ASoC will call pm_runtime_get, but vow don't need */
	for_each_rtdcom(rtd, rtdcom) {
		comp = rtdcom->component;
		pm_runtime_put_autosuspend(comp->dev);
	}
	return 0;
}

static void mt6781_mt6366_vow_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *comp = NULL;
	struct snd_soc_rtdcom_list *rtdcom = NULL;

	dev_info(afe->dev, "%s(), end\n", __func__);
	mt6781_afe_gpio_request(afe, false, MT6781_DAI_VOW, 0);

	/* restore to fool ASoC */
	for_each_rtdcom(rtd, rtdcom) {
		comp = rtdcom->component;
		pm_runtime_get_sync(comp->dev);
	}
}

static const struct snd_soc_ops mt6781_mt6366_vow_ops = {
	.startup = mt6781_mt6366_vow_startup,
	.shutdown = mt6781_mt6366_vow_shutdown,
};
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT

SND_SOC_DAILINK_DEFS(daifmt_playback_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_12,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL12")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_3,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_4,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_5,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_6,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_7,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_playback_8,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_3,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_4,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_5,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_6,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_7,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_8,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_mono_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL_MONO_1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_mono_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL_MONO_2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_capture_mono_3,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL_MONO_3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_lpbk,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless LPBK DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_fm,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless FM DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_speech,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless Speech DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_sph_echo_ref,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Sph_Echo_Ref_DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_spk_init,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Spk_Init_DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_adda_dl_i2s_out,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_ADDA_DL_I2S_OUT DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_src_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_SRC_1_DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_src_bargein,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_SRC_Bargein_DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_primary_codec,
		     DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
		     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "mt6358-snd-codec-aif1")),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_i2s3,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_i2s0,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_i2s1,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_i2s2,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hw_gain_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hw_gain_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hw_src_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hw_src_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_connsys_i2s,
		     DAILINK_COMP_ARRAY(COMP_CPU("CONNSYS_I2S")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_pcm_1,
		     DAILINK_COMP_ARRAY(COMP_CPU("PCM 1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_pcm_2,
		     DAILINK_COMP_ARRAY(COMP_CPU("PCM 2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_ul1,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL1 DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_ul2,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL2 DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_ul3,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL3 DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_ul6,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL6 DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_hw_gain_aaudio,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless HW Gain AAudio DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_hostless_src_aaudio,
		     DAILINK_COMP_ARRAY(COMP_CPU("Hostless SRC AAudio DAI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
SND_SOC_DAILINK_DEFS(daifmt_btcvsd,
		     DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("18050000.mtk-btcvsd-snd")));
#endif
#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
SND_SOC_DAILINK_DEFS(daifmt_offload_playback,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_offload_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("mt_soc_offload_common")));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_voip,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_voip_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_primary,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_primary_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_deepbuf,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_deepbuf_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_playback,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_Playback_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_capture_ul1,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_capture_ul1_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_call_final,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_call_final_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_fast,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_fast_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_ktv,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_ktv_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_capture_raw,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_capture_raw_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_fm_adsp,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_fm_adsp_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(daifmt_dsp_playback_a2dp,
		     DAILINK_COMP_ARRAY(COMP_CPU("audio_task_a2dp_dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif
#ifdef CONFIG_MTK_VOW_SUPPORT
SND_SOC_DAILINK_DEFS(daifmt_vow_capture,
		     DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
		     DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "mt6358-snd-codec-vow")),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT
#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
SND_SOC_DAILINK_DEFS(daifmt_scp_spk_playback,
		     DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_scp_spk")));
#endif
#if defined(CONFIG_MTK_ULTRASND_PROXIMITY)
SND_SOC_DAILINK_DEFS(daifmt_scp_ultra_playback,
		     DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_scp_ultra")));
#endif

static struct snd_soc_dai_link mt6781_mt6366_dai_links[] = {
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_1),
	},
	{
		.name = "Playback_12",
		.stream_name = "Playback_12",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_12),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_2),
	},
	{
		.name = "Playback_3",
		.stream_name = "Playback_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_3),
	},
	{
		.name = "Playback_4",
		.stream_name = "Playback_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_4),
	},
	{
		.name = "Playback_5",
		.stream_name = "Playback_5",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_5),
	},
	{
		.name = "Playback_6",
		.stream_name = "Playback_6",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_6),
	},
	{
		.name = "Playback_7",
		.stream_name = "Playback_7",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_7),
	},
	{
		.name = "Playback_8",
		.stream_name = "Playback_8",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(daifmt_playback_8),
	},
	{
		.name = "Capture_1",
		.stream_name = "Capture_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_1),
	},
	{
		.name = "Capture_2",
		.stream_name = "Capture_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_2),
	},
	{
		.name = "Capture_3",
		.stream_name = "Capture_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_3),
	},
	{
		.name = "Capture_4",
		.stream_name = "Capture_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_4),
	},
	{
		.name = "Capture_5",
		.stream_name = "Capture_5",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_5),
	},
	{
		.name = "Capture_6",
		.stream_name = "Capture_6",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_6),
	},
	{
		.name = "Capture_7",
		.stream_name = "Capture_7",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_7),
	},
	{
		.name = "Capture_8",
		.stream_name = "Capture_8",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_8),
	},
	{
		.name = "Capture_Mono_1",
		.stream_name = "Capture_Mono_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_mono_1),
	},
	{
		.name = "Capture_Mono_2",
		.stream_name = "Capture_Mono_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_mono_2),
	},
	{
		.name = "Capture_Mono_3",
		.stream_name = "Capture_Mono_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(daifmt_capture_mono_3),
	},
	{
		.name = "Hostless_LPBK",
		.stream_name = "Hostless_LPBK",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_lpbk),
	},
	{
		.name = "Hostless_FM",
		.stream_name = "Hostless_FM",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_fm),
	},
	{
		.name = "Hostless_Speech",
		.stream_name = "Hostless_Speech",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_speech),
	},
	{
		.name = "Hostless_Sph_Echo_Ref",
		.stream_name = "Hostless_Sph_Echo_Ref",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_sph_echo_ref),
	},
	{
		.name = "Hostless_Spk_Init",
		.stream_name = "Hostless_Spk_Init",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_spk_init),
	},
	{
		.name = "Hostless_ADDA_DL_I2S_OUT",
		.stream_name = "Hostless_ADDA_DL_I2S_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_adda_dl_i2s_out),
	},
	{
		.name = "Hostless_SRC_1",
		.stream_name = "Hostless_SRC_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_src_1),
	},
	{
		.name = "Hostless_SRC_Bargein",
		.stream_name = "Hostless_SRC_Bargein",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_src_bargein),
	},
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt6781_mt6366_init,
		SND_SOC_DAILINK_REG(daifmt_primary_codec),
	},
	{
		.name = "I2S3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6781_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(daifmt_i2s3),
	},
	{
		.name = "I2S0",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6781_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(daifmt_i2s0),
	},
	{
		.name = "I2S1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6781_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(daifmt_i2s1),
	},
	{
		.name = "I2S2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6781_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(daifmt_i2s2),
	},
	{
		.name = "HW Gain 1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hw_gain_1),
	},
	{
		.name = "HW Gain 2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hw_gain_2),
	},
	{
		.name = "HW_SRC_1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hw_src_1),
	},
	{
		.name = "HW_SRC_2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hw_src_2),
	},
	{
		.name = "CONNSYS_I2S",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_connsys_i2s),
	},
	{
		.name = "PCM 1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_pcm_1),
	},
	{
		.name = "PCM 2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_pcm_2),
	},
	{
		.name = "Hostless_UL1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_ul1),
	},
	{
		.name = "Hostless_UL2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_ul2),
	},
	{
		.name = "Hostless_UL3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_ul3),
	},
	{
		.name = "Hostless_UL6",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_ul6),
	},
	{
		.name = "Hostless_HW_Gain_AAudio",
		.stream_name = "Hostless_HW_Gain_AAudio",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_hw_gain_aaudio),
	},
	{
		.name = "Hostless_SRC_AAudio",
		.stream_name = "Hostless_SRC_AAudio",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(daifmt_hostless_src_aaudio),
	},
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
	{
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		SND_SOC_DAILINK_REG(daifmt_btcvsd),
	},
#endif
#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	{
		.name = "Offload_Playback",
		.stream_name = "Offload_Playback",
		SND_SOC_DAILINK_REG(daifmt_offload_playback),
	},
	{
		.name = "DSP_Playback_Voip",
		.stream_name = "DSP_Playback_Voip",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_voip),
	},
	{
		.name = "DSP_Playback_Primary",
		.stream_name = "DSP_Playback_Primary",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_primary),
	},
	{
		.name = "DSP_Playback_DeepBuf",
		.stream_name = "DSP_Playback_DeepBuf",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_deepbuf),
	},
	{
		.name = "DSP_Playback_Playback",
		.stream_name = "DSP_Playback_Playback",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_playback),
	},
	{
		.name = "DSP_Capture_Ul1",
		.stream_name = "DSP_Capture_Ul1",
		SND_SOC_DAILINK_REG(daifmt_dsp_capture_ul1),
	},
	{
		.name = "DSP_Call_Final",
		.stream_name = "DSP_Call_Final",
		SND_SOC_DAILINK_REG(daifmt_dsp_call_final),
	},
	{
		.name = "DSP_Playback_Fast",
		.stream_name = "DSP_Playback_Fast",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_fast),
	},
	{
		.name = "DSP_Playback_Ktv",
		.stream_name = "DSP_Playback_Ktv",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_ktv),
	},
	{
		.name = "DSP_Capture_Raw",
		.stream_name = "DSP_Capture_Raw",
		SND_SOC_DAILINK_REG(daifmt_dsp_capture_raw),
	},
	{
		.name = "DSP_Playback_Fm_Adsp",
		.stream_name = "DSP_Playback_Fm_Adsp",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_fm_adsp),
	},
	{
		.name = "DSP_Playback_A2DP",
		.stream_name = "DSP_Playback_A2DP",
		SND_SOC_DAILINK_REG(daifmt_dsp_playback_a2dp),
	},
#endif
#ifdef CONFIG_MTK_VOW_SUPPORT
	{
		.name = "VOW_Capture",
		.stream_name = "VOW_Capture",
		.ignore_suspend = 1,
		.ops = &mt6781_mt6366_vow_ops,
		SND_SOC_DAILINK_REG(daifmt_vow_capture),
	},
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT
#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	{
		.name = "SCP_SPK_Playback",
		.stream_name = "SCP_SPK_Playback",
		SND_SOC_DAILINK_REG(daifmt_scp_spk_playback),
	},
#endif
#if defined(CONFIG_MTK_ULTRASND_PROXIMITY)
	{
		.name = "SCP_ULTRA_Playback",
		.stream_name = "SCP_ULTRA_Playback",
		SND_SOC_DAILINK_REG(daifmt_scp_ultra_playback),
	},
#endif
};

static struct snd_soc_card mt6781_mt6366_soc_card = {
	.name = "mt6781-mt6366",
	.owner = THIS_MODULE,
	.dai_link = mt6781_mt6366_dai_links,
	.num_links = ARRAY_SIZE(mt6781_mt6366_dai_links),

	.controls = mt6781_mt6366_controls,
	.num_controls = ARRAY_SIZE(mt6781_mt6366_controls),
	.dapm_widgets = mt6781_mt6366_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6781_mt6366_widgets),
	.dapm_routes = mt6781_mt6366_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6781_mt6366_routes),
};

static int mt6781_mt6366_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6781_mt6366_soc_card;
	struct device_node *platform_node, *codec_node, *spk_node, *dsp_node;
	struct snd_soc_dai_link *spk_out_dai_link, *spk_iv_dai_link;
	int ret, i;
	int spk_out_dai_link_idx, spk_iv_dai_link_idx;
	const char *name;

	dev_info(&pdev->dev, "%s(), ++\n", __func__);

	ret = mtk_spk_update_info(card, pdev,
				  &spk_out_dai_link_idx, &spk_iv_dai_link_idx,
				  &mt6781_mt6366_i2s_ops);
	if (ret) {
		dev_err(&pdev->dev, "%s(), mtk_spk_update_info error\n",
			__func__);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "%s(), update spk dai\n", __func__);

	spk_out_dai_link = &mt6781_mt6366_dai_links[spk_out_dai_link_idx];
	spk_iv_dai_link = &mt6781_mt6366_dai_links[spk_iv_dai_link_idx];
	if (!spk_out_dai_link->codecs->dai_name &&
	    !spk_iv_dai_link->codecs->dai_name) {
		spk_node = of_get_child_by_name(pdev->dev.of_node,
					"mediatek,speaker-codec");
		if (!spk_node) {
			dev_err(&pdev->dev,
				"spk_codec of_get_child_by_name fail\n");
			return -EINVAL;
		}
		ret = snd_soc_of_get_dai_link_codecs(
				&pdev->dev, spk_node, spk_out_dai_link);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"i2s out get_dai_link_codecs fail\n");
			return -EINVAL;
		}
		ret = snd_soc_of_get_dai_link_codecs(
				&pdev->dev, spk_node, spk_iv_dai_link);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"i2s in get_dai_link_codecs fail\n");
			return -EINVAL;
		}
	}

	dev_info(&pdev->dev, "%s(), update platform dai\n", __func__);

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	dsp_node = of_parse_phandle(pdev->dev.of_node,
				    "mediatek,snd_audio_dsp", 0);
	if (!dsp_node)
		dev_info(&pdev->dev, "Property 'snd_audio_dsp' missing or invalid\n");

	for (i = 0; i < card->num_links; i++) {
		if (mt6781_mt6366_dai_links[i].platforms->name)
			continue;
		/* no platform assign and with dsp playback node. */
		name = mt6781_mt6366_dai_links[i].name;
		if (!strncmp(name, "DSP", strlen("DSP")) &&
		    mt6781_mt6366_dai_links[i].platforms->name == NULL) {
			mt6781_mt6366_dai_links[i].platforms->of_node = dsp_node;
			continue;
		}
		mt6781_mt6366_dai_links[i].platforms->of_node = platform_node;
	}

	dev_info(&pdev->dev, "%s(), update audio-codec dai\n", __func__);

	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		return -EINVAL;
	}
	for (i = 0; i < card->num_links; i++) {
		if (mt6781_mt6366_dai_links[i].codecs->name ||
		    i == spk_out_dai_link_idx ||
		    i == spk_iv_dai_link_idx)
			continue;
		mt6781_mt6366_dai_links[i].codecs->of_node = codec_node;
	}

	card->dev = &pdev->dev;
#if defined(CONFIG_SND_SOC_SIA81XX)
	ret = soc_aux_init_only_sia81xx(pdev, card);
	if (ret)
		dev_err(&pdev->dev, "%s soc_aux_init_only_sia81xx fail %d\n",
			__func__, ret);
#endif
	dev_info(&pdev->dev, "%s(), devm_snd_soc_register_card\n", __func__);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id mt6781_mt6366_dt_match[] = {
	{.compatible = "mediatek,mt6781-mt6366-sound",},
	{}
};
#endif

static const struct dev_pm_ops mt6781_mt6366_pm_ops = {
	.poweroff = snd_soc_poweroff,
	.restore = snd_soc_resume,
};

static struct platform_driver mt6781_mt6366_driver = {
	.driver = {
		.name = "mt6781-mt6366",
#ifdef CONFIG_OF
		.of_match_table = mt6781_mt6366_dt_match,
#endif
		.pm = &mt6781_mt6366_pm_ops,
	},
	.probe = mt6781_mt6366_dev_probe,
};

module_platform_driver(mt6781_mt6366_driver);

/* Module information */
MODULE_DESCRIPTION("MT6781 MT6366 ALSA SoC machine driver");
MODULE_AUTHOR("Patrick Cheng <Cheng-yi.Cheng@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt6781 mt6366 soc card");
