/*
 * uda134x.c  --  UDA134X ALSA SoC Codec driver
 *
 * Modifications by Christian Pellegrin <chripell@evolware.org>
 *
 * Copyright 2007 Dension Audio Systems Ltd.
 * Author: Zoltan Devai
 *
 * Based on the WM87xx drivers by Liam Girdwood and Richard Purdie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>

#include <sound/uda134x.h>
#include <sound/l3.h>

#include "uda134x.h"


#define UDA134X_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
                SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |\
                SNDRV_PCM_RATE_48000)

#define UDA134X_FORMATS (SNDRV_PCM_FMTBIT_S8 |\
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

	struct snd_pcm_substream *master_substream;
	struct snd_pcm_substream *slave_substream;
};

/* In-data addresses are hard-coded into the reg-cache values */
static const char uda134x_reg[UDA134X_REGS_NUM] = {
	/* Extended address registers */
	0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Status, data regs */
	0x00, 0x83, 0x00, 0x40, 0x80, 0xC0, 0x00,
};

/*
 * The codec has no support for reading its registers except for peak level...
 */
static inline unsigned int uda134x_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u8 *cache = (u8 *)uda134x_reg;
	if (reg >= UDA134X_REGS_NUM)
		return -1;
	return cache[reg];
}


/*
 * The codec has no support for reading its registers except for peak level...
 */
/*
	FIXME
	i2c read unsigned short;but uda134x_read int.   
*/
static inline unsigned int uda134x_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
//	return 	ls1a_i2c_read(reg );
}


/* * Write to the uda134x registers */
static int uda134x_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
//	ls1a_i2c_write(reg, value);
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


static struct snd_soc_dai_driver uda134x_dai = {
	.name = "uda134x-hifi",
	/* playback capabilities */
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = UDA134X_RATES,
		.formats = UDA134X_FORMATS,
	},
	/* capture capabilities */
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = UDA134X_RATES,
		.formats = UDA134X_FORMATS,
	},
};

static inline void uda134x_reset(struct snd_soc_codec *codec)
{
}

static int uda134x_soc_probe(struct snd_soc_codec *codec)
{
	u8 reg;
	struct snd_ac97_bus *ac97_bus;
	struct snd_ac97_template ac97_template;
	int ret;
	
//	uda134x_reset(codec);
//	reg = uda134x_read_reg_cache(codec, UDA134X_STATUS1);
//	uda134x_write(codec, UDA134X_STATUS1, reg | 0x03);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_uda134x = {
	.probe = uda134x_soc_probe,
//	.read = uda134x_read,
//	.write = uda134x_write,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
#include <linux/i2c.h>
#define UDA1342 0
#define ES8388 1

static int write_reg(struct i2c_client *client, int reg, int value)
{
	/* UDA1342 wants MSB first, but SMBus sends LSB first */
	i2c_smbus_write_word_data(client, reg, swab16(value));
	return 0;
}

static __devinit int i2c_codecs_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	if (id->driver_data == UDA1342) {
		pr_info("audio codec:uda1342\n");
		write_reg(client, 0x00, 0x8000); /* reset registers */
		udelay(500);
		write_reg(client, 0x00, 0x1241); /* select input 1 */

//		write_reg(client, 0x00, 0x5c42);
		write_reg(client, 0x00, 0x1c42);
//		write_reg(client, 0x01, 0x0004);
		write_reg(client, 0x01, 0x0045);
		write_reg(client, 0x10, 0x0002);
		write_reg(client, 0x11, 0x0000);
		write_reg(client, 0x12, 0x0000);
		write_reg(client, 0x20, 0x0f30);
		write_reg(client, 0x21, 0x0f30);
	} else if (id->driver_data == ES8388) {	/* 需要i2c总线时钟设置为100KHz */
		pr_info("audio codec:es8388\n");
		i2c_smbus_write_byte_data(client, 0x08, 0x00);
		i2c_smbus_write_byte_data(client, 0x2b, 0x80);
		i2c_smbus_write_byte_data(client, 0x00, 0x32);
		i2c_smbus_write_byte_data(client, 0x01, 0x72);
		i2c_smbus_write_byte_data(client, 0x03, 0x00);
		i2c_smbus_write_byte_data(client, 0x04, 0x3c);
		i2c_smbus_write_byte_data(client, 0x09, 0x00);
		i2c_smbus_write_byte_data(client, 0x0a, 0x00);
		i2c_smbus_write_byte_data(client, 0x0c, 0x0c);
		i2c_smbus_write_byte_data(client, 0x0d, 0x02);
		i2c_smbus_write_byte_data(client, 0x0f, 0x70);
		i2c_smbus_write_byte_data(client, 0x10, 0x00);
		i2c_smbus_write_byte_data(client, 0x11, 0x00);
		i2c_smbus_write_byte_data(client, 0x17, 0x18);
		i2c_smbus_write_byte_data(client, 0x18, 0x02);
		i2c_smbus_write_byte_data(client, 0x19, 0x72);
		i2c_smbus_write_byte_data(client, 0x1a, 0x00);
		i2c_smbus_write_byte_data(client, 0x1b, 0x00);
		i2c_smbus_write_byte_data(client, 0x26, 0x00);
		i2c_smbus_write_byte_data(client, 0x27, 0xb8);
		i2c_smbus_write_byte_data(client, 0x28, 0x38);
		i2c_smbus_write_byte_data(client, 0x29, 0x38);
		i2c_smbus_write_byte_data(client, 0x2a, 0xd0);
		i2c_smbus_write_byte_data(client, 0x2e, 0x1e);
		i2c_smbus_write_byte_data(client, 0x2f, 0x1e);
		i2c_smbus_write_byte_data(client, 0x30, 0x1e);
		i2c_smbus_write_byte_data(client, 0x31, 0x1e);
		i2c_smbus_write_byte_data(client, 0x02, 0x00);
	} else {
		pr_info("no audio codec\n");
	}

	return snd_soc_register_codec(&client->dev, &soc_codec_dev_uda134x, &uda134x_dai, 1);

//	return 0;
}

static int __devexit i2c_codecs_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

static const struct i2c_device_id i2c_codecs_id[] = {
	{ "uda1342", UDA1342, },
	{ "es8388", ES8388, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_codecs_id);

static struct i2c_driver i2c_codecs_driver = {
	.driver = {
		.name =  "uda1342-codec",
		.owner = THIS_MODULE,
	},
	.probe =    i2c_codecs_probe,
	.remove =   __devexit_p(i2c_codecs_remove),
	.id_table = i2c_codecs_id,
};
#endif

static int __init uda134x_codec_init(void)
{
	int ret = 0;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&i2c_codecs_driver);
	if (ret != 0)
		pr_err("Failed to register I2C codecs driver: %d\n", ret);		
#endif

//	ret = platform_driver_register(&uda134x_codec_driver);
//	if (ret)
//		printk(KERN_ERR "failed to register ls1x-audio\n");

	return ret;
//	return platform_driver_register(&uda134x_codec_driver);
}
module_init(uda134x_codec_init);

static void __exit uda134x_codec_exit(void)
{
//	platform_driver_unregister(&uda134x_codec_driver);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&i2c_codecs_driver);
#endif
}
module_exit(uda134x_codec_exit);

MODULE_DESCRIPTION("UDA134X ALSA soc codec driver");
MODULE_AUTHOR("Zoltan Devai, Christian Pellegrin <chripell@evolware.org>");
MODULE_LICENSE("GPL");
