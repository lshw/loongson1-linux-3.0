/*
 * uda134x.c  --  UDA1342 ALSA SoC Codec driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <sound/uda1342.h>

#include "uda1342.h"


#define UDA1342_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
                SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |\
                SNDRV_PCM_RATE_48000)

#define UDA1342_FORMATS (SNDRV_PCM_FMTBIT_S8 |\
                               SNDRV_PCM_FMTBIT_S16_LE |\
                               SNDRV_PCM_FMTBIT_S16_BE |\
                               SNDRV_PCM_FMTBIT_S20_3LE |\
                               SNDRV_PCM_FMTBIT_S20_3BE |\
                               SNDRV_PCM_FMTBIT_S24_3LE |\
                               SNDRV_PCM_FMTBIT_S24_3BE |\
                               SNDRV_PCM_FMTBIT_S32_LE |\
			       SNDRV_PCM_FMTBIT_U16_LE	|\
                               SNDRV_PCM_FMTBIT_S32_BE)

struct uda1342_priv {
	int sysclk;
	int dai_fmt;
	void *control_data;

	struct snd_pcm_substream *master_substream;
	struct snd_pcm_substream *slave_substream;
};

/* In-data addresses are hard-coded into the reg-cache values */
static const u16 uda1342_reg[UDA1342_REGS_NUM] = {
	0x1c42, 0x0045, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0f30, 0x0f30, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

/*
 * The codec has no support for reading its registers except for peak level...
 */
static inline unsigned int uda1342_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;

	if (reg >= UDA1342_REGS_NUM)
		return -1;
	return cache[reg];
}

/*
 * Write the register cache
 */
static inline void uda1342_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;

	if (reg >= UDA1342_REGS_NUM)
		return;
	cache[reg] = value;
}

/*
 * Write to the uda1342 registers
 *
 */
static int uda1342_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct uda1342_platform_data *pd = codec->dev->platform_data;
	u8 data[3];

	/* data is
	 *   data[0] is register offset
	 *   data[1] is MS byte
	 *   data[2] is LS byte
	 */
	data[0] = reg;
	data[1] = (value & 0xff00) >> 8;
	data[2] = value & 0x00ff;

	pr_debug("%s reg: %02X, value:%02X\n", __func__, reg, value);

	if (reg >= UDA1342_REGS_NUM) {
		printk(KERN_ERR "%s unknown register: reg: %u",
		       __func__, reg);
		return -EINVAL;
	}

	uda1342_write_reg_cache(codec, reg, value);

	/* the interpolator & decimator regs must only be written when the
	 * codec DAI is active.
	 */
	if (!codec->active)
		return 0;
	pr_debug("uda1380: hw write %x val %x\n", reg, value);
	if (codec->hw_write(codec->control_data, data, 3) == 3) {
		unsigned int val;
		i2c_master_send(codec->control_data, data, 1);
		i2c_master_recv(codec->control_data, data, 2);
		val = (data[0]<<8) | data[1];
		if (val != value) {
			pr_debug("uda1380: READ BACK VAL %x\n",
					(data[0]<<8) | data[1]);
			return -EIO;
		}
		return 0;
	} else
		return -EIO;

	return 0;
}

static inline void uda1342_reset(struct snd_soc_codec *codec)
{
	u16 reset_reg = uda1342_read_reg_cache(codec, UDA1342_REG00);
	uda1342_write(codec, UDA1342_REG00, reset_reg | (1<<15));
	msleep(1);
	uda1342_write(codec, UDA1342_REG00, reset_reg & ~(1<<15));
}

static int uda1342_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = uda1342_read_reg_cache(codec, UDA1342_REG10);

	pr_debug("%s mute: %d\n", __func__, mute);

	if (mute)
		mute_reg |= (0x7<<4);
	else
		mute_reg &= ~(0x7<<4);

	uda1342_write(codec, UDA1342_REG10, mute_reg);

	return 0;
}

static int uda1342_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec =rtd->codec;
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);
	struct snd_pcm_runtime *master_runtime;

	if (uda1342->master_substream) {
		master_runtime = uda1342->master_substream->runtime;

		pr_debug("%s constraining to %d bits at %d\n", __func__,
			 master_runtime->sample_bits,
			 master_runtime->rate);

		snd_pcm_hw_constraint_minmax(substream->runtime,
					     SNDRV_PCM_HW_PARAM_RATE,
					     master_runtime->rate,
					     master_runtime->rate);

		snd_pcm_hw_constraint_minmax(substream->runtime,
					     SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
					     master_runtime->sample_bits,
					     master_runtime->sample_bits);

		uda1342->slave_substream = substream;
	} else
		uda1342->master_substream = substream;

	return 0;
}

static void uda1342_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);

	if (uda1342->master_substream == substream)
		uda1342->master_substream = uda1342->slave_substream;

	uda1342->slave_substream = NULL;
}

static int uda1342_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);
	u16 hw_params;

	if (substream == uda1342->slave_substream) {
		pr_debug("%s ignoring hw_params for slave substream\n",
			 __func__);
		return 0;
	}

	hw_params = uda1342_read_reg_cache(codec, UDA1342_REG00);
//	hw_params &= STATUS0_SYSCLK_MASK;
	hw_params &= STATUS0_DAIFMT_MASK;

	pr_debug("%s dai_fmt: %d, params_format:%d\n", __func__,
		 uda1342->dai_fmt, params_format(params));

	/* set DAI format and word length */
	switch (uda1342->dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			hw_params |= (1<<2);
			break;
		case SNDRV_PCM_FORMAT_S18_3LE:
			hw_params |= (1<<2);
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			hw_params |= ((1<<2) | (1<<1));
			break;
		default:
			printk(KERN_ERR "%s unsupported format (right)\n",
			       __func__);
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		hw_params |= (1<<4);
		break;
	default:
		printk(KERN_ERR "%s unsupported format\n", __func__);
		return -EINVAL;
	}

	uda1342_write(codec, UDA1342_REG00, hw_params);

	return 0;
}

static int uda1342_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s clk_id: %d, freq: %u, dir: %d\n", __func__,
		 clk_id, freq, dir);

	/* Anything between 256fs*8Khz and 512fs*48Khz should be acceptable
	   because the codec is slave. Of course limitations of the clock
	   master (the IIS controller) apply.
	   We'll error out on set_hw_params if it's not OK */
	if ((freq >= (256 * 8000)) && (freq <= (768 * 96000))) {
		uda1342->sysclk = freq;
		return 0;
	}

	printk(KERN_ERR "%s unsupported sysclk\n", __func__);
	return -EINVAL;
}

static int uda1342_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s fmt: %08X\n", __func__, fmt);

	/* codec supports only full slave mode */
	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		printk(KERN_ERR "%s unsupported slave mode\n", __func__);
		return -EINVAL;
	}

	/* no support for clock inversion */
	if ((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_NF) {
		printk(KERN_ERR "%s unsupported clock inversion\n", __func__);
		return -EINVAL;
	}

	/* We can't setup DAI format here as it depends on the word bit num */
	/* so let's just store the value for later */
	uda1342->dai_fmt = fmt;

	return 0;
}

static int uda1342_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	u16 reg;
	struct uda1342_platform_data *pd = codec->dev->platform_data;
	int i;
	u16 *cache = codec->reg_cache;

	pr_debug("%s bias level %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* ADC, DAC on */
		reg = uda1342_read_reg_cache(codec, UDA1342_REG00);
		uda1342_write(codec, UDA1342_REG00, reg | 0x0002);
		break;
	case SND_SOC_BIAS_PREPARE:
		/* power on */
		if (pd->power) {
			pd->power(1);
			/* Sync reg_cache with the hardware */
			for (i = 0; i < ARRAY_SIZE(uda1342_reg); i++)
				codec->driver->write(codec, i, *cache++);
		}
		break;
	case SND_SOC_BIAS_STANDBY:
		/* ADC, DAC power off */
		reg = uda1342_read_reg_cache(codec, UDA1342_REG00);
		uda1342_write(codec, UDA1342_REG00, reg & ~(0x0e02));
		break;
	case SND_SOC_BIAS_OFF:
		/* power off */
		if (pd->power)
			pd->power(0);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

/* declarations of ALSA reg_elem_REAL controls */
static const char *uda1342_deemp[] = {
	"None",
	"32kHz",
	"44.1kHz",
	"48kHz",
	"96kHz",
};
static const char *uda1342_spf_mode[] = {
	"Flat",
	"Minimum1",
	"Minimum2",
	"Maximum"
};
static const char *uda1342_mix_control[] = {
	"off",
	"PCM only",
	"before sound processing",
	"after sound processing"
};
static const char *uda1342_sdet_setting[] = {
	"3200",
	"4800",
	"9600",
	"19200"
};
static const char *uda1342_os_setting[] = {
	"single-speed",
	"double-speed (no mixing)",
	"quad-speed (no mixing)"
};

static const struct soc_enum uda1342_deemp_enum = 
	SOC_ENUM_SINGLE(UDA1342_REG10, 0, 5, uda1342_deemp);
static const struct soc_enum uda1342_spf_enum =
	SOC_ENUM_SINGLE(UDA1342_REG10, 14, 4, uda1342_spf_mode);		/* M */
static const struct soc_enum uda1342_mix_enum =
	SOC_ENUM_SINGLE(UDA1342_REG01, 4, 4, uda1342_mix_control);	/* MIX, MIX_POS */
static const struct soc_enum uda1342_sdet_enum =
	SOC_ENUM_SINGLE(UDA1342_REG01, 2, 4, uda1342_sdet_setting);	/* SD_VALUE */
static const struct soc_enum uda1342_os_enum =
	SOC_ENUM_SINGLE(UDA1342_REG01, 6, 3, uda1342_os_setting);	/* OS */

/*
 * from -78 dB in 1 dB steps (3 dB steps, really. LSB are ignored),
 * from -66 dB in 0.5 dB steps (2 dB steps, really) and
 * from -52 dB in 0.25 dB steps
 */
static const unsigned int mvol_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 15, TLV_DB_SCALE_ITEM(-8200, 100, 1),
	16, 43, TLV_DB_SCALE_ITEM(-6600, 50, 0),
	44, 252, TLV_DB_SCALE_ITEM(-5200, 25, 0),
};

/*
 * from -48 dB in 1.5 dB steps (mute instead of -49.5 dB)
 */
static DECLARE_TLV_DB_SCALE(amix_tlv, -4950, 150, 1);


/* from 0 to 6 dB in 2 dB steps if SPF mode != flat */
static DECLARE_TLV_DB_SCALE(tr_tlv, 0, 200, 0);

/* from 0 to 24 dB in 2 dB steps, if SPF mode == maximum, otherwise cuts
 * off at 18 dB max) */
static DECLARE_TLV_DB_SCALE(bb_tlv, 0, 200, 0);

/* from -63 to 24 dB in 0.5 dB steps (-128...48) */
static DECLARE_TLV_DB_SCALE(dec_tlv, -6400, 50, 1);

/* from 0 to 24 dB in 3 dB steps */
static DECLARE_TLV_DB_SCALE(pga_tlv, 0, 300, 0);

static const struct snd_kcontrol_new uda1341_snd_controls[] = {
	SOC_DOUBLE_TLV("Master Playback Volume", UDA1342_REG11, 0, 8, 255, 1, mvol_tlv),	/* MVCL, MVCR */
	SOC_DOUBLE_TLV("Analog Mixer Volume", UDA1342_REG12, 0, 8, 255, 1, amix_tlv),	/* AVCR, AVCL */
	SOC_ENUM("Sound Processing Filter", uda1342_spf_enum),				/* M */
	SOC_SINGLE_TLV("Tone Control - Treble", UDA1342_REG10, 8, 3, 0, tr_tlv), 	/* TR */
	SOC_SINGLE_TLV("Tone Control - Bass", UDA1342_REG10, 10, 15, 0, bb_tlv),	/* BB */
	SOC_SINGLE("Master Playback Switch", UDA1342_REG10, 4, 1, 1),		/* MTM */
	SOC_SINGLE("ADC Playback Switch", UDA1342_REG10, 6, 1, 1),		/* MTB from decimation filter */
	SOC_SINGLE("PCM Playback Switch", UDA1342_REG10, 5, 1, 1),		/* MTA, from digital data input */
	SOC_ENUM("ADC Playback De-emphasis", uda1342_deemp_enum),		/* DE */
	SOC_SINGLE("DAC Polarity inverting Switch", UDA1342_REG00, 0, 1, 0),	/* DA_POL_INV */
	SOC_SINGLE("ADC Polarity inverting Switch", UDA1342_REG00, 8, 1, 0),	/* ADCPOL_INV */
	SOC_ENUM("Digital Mixer Signal Control", uda1342_mix_enum),		/* MIX_POS, MIX */
	SOC_SINGLE("Silence Detector Switch", UDA1342_REG10, 7, 1, 0),		/* SDET_ON */
	SOC_ENUM("Silence Detector Setting", uda1342_sdet_enum),		/* SD_VALUE */
	SOC_ENUM("Oversampling Input", uda1342_os_enum),			/* OS */
	SOC_SINGLE_TLV("ADC1 Mixer Volume", UDA1342_REG20, 0, 48, 0, dec_tlv),	/* DAC channel 1 */
	SOC_SINGLE_TLV("ADC2 Mixer Volume", UDA1342_REG21, 0, 48, 0, dec_tlv),	/* DAC channel 2 */
	SOC_SINGLE_TLV("Line1 Capture Volume", UDA1342_REG20, 8, 8, 0, pga_tlv), /* Line1 */
	SOC_SINGLE_TLV("Line2 Capture Volume", UDA1342_REG21, 8, 8, 0, pga_tlv), /* Line2 */
	SOC_SINGLE("DC Filter Bypass Switch", UDA1342_REG00, 13, 1, 0),		/* SKIP_DCFIL (before decimator) */
	SOC_SINGLE("DC Filter Enable Switch", UDA1342_REG00, 12, 1, 0),		/* EN_DCFIL (at output of decimator) */
};

static struct snd_soc_dai_ops uda1342_dai_ops = {
	.startup	= uda1342_startup,
	.shutdown	= uda1342_shutdown,
	.hw_params	= uda1342_hw_params,
	.digital_mute	= uda1342_mute,
	.set_sysclk	= uda1342_set_dai_sysclk,
	.set_fmt	= uda1342_set_dai_fmt,
};

static struct snd_soc_dai_driver uda1342_dai[] = {
{
	.name = "uda1342-hifi",
	/* playback capabilities */
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = UDA1342_RATES,
		.formats = UDA1342_FORMATS,
	},
	/* capture capabilities */
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = UDA1342_RATES,
		.formats = UDA1342_FORMATS,
	},
	/* pcm operations */
	.ops = &uda1342_dai_ops,
},
};

static int uda1342_soc_probe(struct snd_soc_codec *codec)
{
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);
	struct uda1342_platform_data *pd = codec->dev->platform_data;

	int ret;

	printk(KERN_INFO "UDA1342 SoC Audio Codec\n");

	if (!pd) {
		printk(KERN_ERR "UDA1342 SoC codec: "
		       "missing L3 bitbang function\n");
		return -ENODEV;
	}

	snd_soc_codec_set_drvdata(codec, uda1342);

	codec->hw_write = (hw_write_t)i2c_master_send;
	codec->control_data = uda1342->control_data;

	if (pd->power)
		pd->power(1);

	uda1342_reset(codec);

	if (pd->is_powered_on_standby)
		uda1342_set_bias_level(codec, SND_SOC_BIAS_ON);
	else
		uda1342_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	ret = snd_soc_add_controls(codec, uda1341_snd_controls,
					ARRAY_SIZE(uda1341_snd_controls));
	if (ret < 0) {
		printk(KERN_ERR "UDA1342: failed to register controls\n");
		return ret;
	}

	return 0;
}

/* power down chip */
static int uda1342_soc_remove(struct snd_soc_codec *codec)
{
	struct uda1342_priv *uda1342 = snd_soc_codec_get_drvdata(codec);

	uda1342_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	uda1342_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(uda1342);
	return 0;
}

#if defined(CONFIG_PM)
static int uda1342_soc_suspend(struct snd_soc_codec *codec,
						pm_message_t state)
{
	uda1342_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	uda1342_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int uda1342_soc_resume(struct snd_soc_codec *codec)
{
	uda1342_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	uda1342_set_bias_level(codec, SND_SOC_BIAS_ON);
	return 0;
}
#else
#define uda1342_soc_suspend NULL
#define uda1342_soc_resume NULL
#endif /* CONFIG_PM */

static struct snd_soc_codec_driver soc_codec_dev_uda1342 = {
	.probe = uda1342_soc_probe,
	.remove =       uda1342_soc_remove,
	.suspend =      uda1342_soc_suspend,
	.resume =       uda1342_soc_resume,
	.reg_cache_size = sizeof(uda1342_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = uda1342_reg,
	.reg_cache_step = 1,
	.read = uda1342_read_reg_cache,
	.write = uda1342_write,
	.set_bias_level = uda1342_set_bias_level,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static __devinit int uda1342_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct uda1342_priv *uda1342;
	int ret;

	uda1342 = kzalloc(sizeof(struct uda1342_priv), GFP_KERNEL);
	if (uda1342 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, uda1342);
	uda1342->control_data = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_uda1342, &uda1342_dai, ARRAY_SIZE(uda1342_dai));
	if (ret < 0)
		kfree(uda1342);
	return ret;
}

static int __devexit uda1342_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

static const struct i2c_device_id uda1342_i2c_id[] = {
	{ "uda1342", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, uda1342_i2c_id);

static struct i2c_driver uda1342_i2c_driver = {
	.driver = {
		.name =  "uda1342-codec",
		.owner = THIS_MODULE,
	},
	.probe =    uda1342_i2c_probe,
	.remove =   __devexit_p(uda1342_i2c_remove),
	.id_table = uda1342_i2c_id,
};
#endif

static int __init uda1342_codec_init(void)
{
	int ret;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&uda1342_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register UDA1342 I2C driver: %d\n", ret);
#endif
	return 0;
}
module_init(uda1342_codec_init);

static void __exit uda1342_codec_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&uda1342_i2c_driver);
#endif
}
module_exit(uda1342_codec_exit);

MODULE_DESCRIPTION("UDA1342 ALSA soc codec driver");
MODULE_AUTHOR("Tang Haifeng <tanghaifeng-gz@loongson.cn>");
MODULE_LICENSE("GPL");
