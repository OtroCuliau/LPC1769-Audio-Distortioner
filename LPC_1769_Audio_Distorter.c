/*
===============================================================================
 Name        : TPF_V0.c
 Author      : $Raimondi
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================

	@brief:
			Trabajo final de ED3. Se pretende hacer un mezclador de señales, tipo pedalera de guitarra
			que introduzca efectos sobre la señal de audio de entrada y proporcione una salida para ir
			a un amplificador de audio.

			La salida de una guitarra se encuentra en el rango de los 200[mV] Vp, por lo que se
			implementara una etapa de acondicionamiento de señal, para llevar la señal de entrada a un
			rango de trabajo entre 0-3.3 [V] (rango en el que trabaja el ADC).

			Las señales de audio tienen una frecuencia maxima de alrededor de 20[kHz] por lo que se
			tomaran muestras a una frecuencia de 200[kHz] para tener buena fidelidad. Esto se logra
			haciendo una interrupcion con ADC en modo BURST.

			Una mejora para el futuro podria ser hacer la modulacion con distintas señales (cuadrada, 
			diente de sierra, etc..) que esten guardadas en memoria.

			Otra modificacion, usar dos canales de ADC y a la salida mostrar el resultado del
			producto o suma de ambas señales. Una de las señales podria ser una señal estatica 
			(ej. un seno de frecuencia preestablecida por el usuario).

	Descripcion de los efectos:

			1) No hay efecto
			2) Bit-crusher: De acuerdo al valor de una variable (bit_crush) se realiza la operacion
							bitwise (<<) de la señal tantas posiciones como sea el valor de la var.
			3) Distorsion:	De acuerdo al valor de una variable (distortion) se fija un umbral, que
							si es superado por la señal de entrada, se trunca la señal a ese valor.
			4) Fuzz:		De acuerdo al valor de una variable (fuzz) si la señal de entrada supera
							ese nivel, la salida se ata a Vcc (3.3 [V]) o Vss (0 [V]).
			5) Modulacion: 	Se modula la señal de entrada con un seno de 10b. Se utiliza la variable
							(sample) para indicar la posición del seno y la variable (speed) para
							modificar la frecuencia del seno (aumenta el salto que da la variable
							(sample) en las muestras, si va de 1 en 1 o de 2 en 2 etc...)
			6) Modulacion Triangulo: igual que la anterior pero con una señal triangular.
*/

/*	Librerias a utilizar*/
#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <lpc17xx_pinsel.h>
#include <lpc17xx_adc.h>
#include <lpc17xx_dac.h>
#include <lpc17xx_timer.h>
#include <lpc17xx_exti.h>
#include <stdio.h>

/* Constantes definidas a priori*/

#define CHANNEL_0	((uint8_t)	0)

/*	Funciones*/

void configADC()		;
void configDAC()	 	;
void configPins()		;
void ADC_IRQHandler()	;
void EINT1_IRQHandler()	;
void EINT2_IRQHandler()	;
void EINT3_IRQHandler()	;
uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);

/*	Variables globales*/

uint16_t ADC0Value 	= 0		;	/*	Var. para guardar el valor de conversion del ADC*/
uint8_t  filtro 	= 0		;	/*	Bandera para ver el filtro a aplicar */
uint16_t bit_crush 	= 0		;	/*	Var. para efecto bit crusher*/
uint16_t distortion	= 200	;	/*	Var. para efecto de distorsion (valor inicial aleatorio)*/
uint16_t fuzz		= 250	;	/*	Var. para efecto de fuzz (valor inicial aleatorio)*/
uint16_t sample		= 0		;	/*	Var. para la distorsion por modulacion con el seno*/
uint16_t speed		= 0		;	/*	Var. para modificar la frecuencia del seno*/
uint8_t	 divider	= 0		;	/*	Var. para dividir la frec. del seno*/
const uint16_t seno[1024]=		/*	Tabla del seno que se utiliza para la modulacion*/
{
		0x200,0x203,0x206,0x209,0x20c,0x20f,0x212,0x215,0x219,0x21c,0x21f,0x222,0x225,0x228,0x22b,0x22f,0x232,0x235,0x238,0x23b,
		0x23e,0x241,0x244,0x248,0x24b,0x24e,0x251,0x254,0x257,0x25a,0x25d,0x260,0x263,0x266,0x26a,0x26d,0x270,0x273,0x276,0x279,
		0x27c,0x27f,0x282,0x285,0x288,0x28b,0x28e,0x291,0x294,0x297,0x29a,0x29d,0x2a0,0x2a3,0x2a6,0x2a9,0x2ac,0x2af,0x2b2,0x2b5,
		0x2b8,0x2bb,0x2be,0x2c1,0x2c3,0x2c6,0x2c9,0x2cc,0x2cf,0x2d2,0x2d5,0x2d8,0x2da,0x2dd,0x2e0,0x2e3,0x2e6,0x2e8,0x2eb,0x2ee,
		0x2f1,0x2f4,0x2f6,0x2f9,0x2fc,0x2ff,0x301,0x304,0x307,0x309,0x30c,0x30f,0x311,0x314,0x317,0x319,0x31c,0x31f,0x321,0x324,
		0x326,0x329,0x32b,0x32e,0x330,0x333,0x335,0x338,0x33a,0x33d,0x33f,0x342,0x344,0x347,0x349,0x34b,0x34e,0x350,0x353,0x355,
		0x357,0x35a,0x35c,0x35e,0x360,0x363,0x365,0x367,0x369,0x36c,0x36e,0x370,0x372,0x374,0x377,0x379,0x37b,0x37d,0x37f,0x381,
		0x383,0x385,0x387,0x389,0x38b,0x38d,0x38f,0x391,0x393,0x395,0x397,0x399,0x39b,0x39c,0x39e,0x3a0,0x3a2,0x3a4,0x3a6,0x3a7,
		0x3a9,0x3ab,0x3ad,0x3ae,0x3b0,0x3b2,0x3b3,0x3b5,0x3b6,0x3b8,0x3ba,0x3bb,0x3bd,0x3be,0x3c0,0x3c1,0x3c3,0x3c4,0x3c6,0x3c7,
		0x3c9,0x3ca,0x3cb,0x3cd,0x3ce,0x3cf,0x3d1,0x3d2,0x3d3,0x3d5,0x3d6,0x3d7,0x3d8,0x3d9,0x3db,0x3dc,0x3dd,0x3de,0x3df,0x3e0,
		0x3e1,0x3e2,0x3e3,0x3e4,0x3e5,0x3e6,0x3e7,0x3e8,0x3e9,0x3ea,0x3eb,0x3ec,0x3ed,0x3ed,0x3ee,0x3ef,0x3f0,0x3f1,0x3f1,0x3f2,
		0x3f3,0x3f3,0x3f4,0x3f5,0x3f5,0x3f6,0x3f6,0x3f7,0x3f8,0x3f8,0x3f9,0x3f9,0x3fa,0x3fa,0x3fa,0x3fb,0x3fb,0x3fc,0x3fc,0x3fc,
		0x3fd,0x3fd,0x3fd,0x3fd,0x3fe,0x3fe,0x3fe,0x3fe,0x3fe,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,0x3ff,
		0x3ff,0x3ff,0x3ff,0x3fe,0x3fe,0x3fe,0x3fe,0x3fe,0x3fe,0x3fd,0x3fd,0x3fd,0x3fc,0x3fc,0x3fc,0x3fb,0x3fb,0x3fb,0x3fa,0x3fa,
		0x3f9,0x3f9,0x3f8,0x3f8,0x3f7,0x3f7,0x3f6,0x3f6,0x3f5,0x3f4,0x3f4,0x3f3,0x3f2,0x3f2,0x3f1,0x3f0,0x3ef,0x3ef,0x3ee,0x3ed,
		0x3ec,0x3eb,0x3eb,0x3ea,0x3e9,0x3e8,0x3e7,0x3e6,0x3e5,0x3e4,0x3e3,0x3e2,0x3e1,0x3e0,0x3df,0x3de,0x3dc,0x3db,0x3da,0x3d9,
		0x3d8,0x3d6,0x3d5,0x3d4,0x3d3,0x3d1,0x3d0,0x3cf,0x3cd,0x3cc,0x3cb,0x3c9,0x3c8,0x3c6,0x3c5,0x3c4,0x3c2,0x3c1,0x3bf,0x3be,
		0x3bc,0x3ba,0x3b9,0x3b7,0x3b6,0x3b4,0x3b2,0x3b1,0x3af,0x3ad,0x3ac,0x3aa,0x3a8,0x3a6,0x3a5,0x3a3,0x3a1,0x39f,0x39d,0x39c,
		0x39a,0x398,0x396,0x394,0x392,0x390,0x38e,0x38c,0x38a,0x388,0x386,0x384,0x382,0x380,0x37e,0x37c,0x37a,0x378,0x375,0x373,
		0x371,0x36f,0x36d,0x36b,0x368,0x366,0x364,0x362,0x35f,0x35d,0x35b,0x358,0x356,0x354,0x351,0x34f,0x34d,0x34a,0x348,0x345,
		0x343,0x341,0x33e,0x33c,0x339,0x337,0x334,0x332,0x32f,0x32d,0x32a,0x328,0x325,0x322,0x320,0x31d,0x31b,0x318,0x315,0x313,
		0x310,0x30d,0x30b,0x308,0x305,0x303,0x300,0x2fd,0x2fa,0x2f8,0x2f5,0x2f2,0x2ef,0x2ed,0x2ea,0x2e7,0x2e4,0x2e1,0x2df,0x2dc,
		0x2d9,0x2d6,0x2d3,0x2d0,0x2ce,0x2cb,0x2c8,0x2c5,0x2c2,0x2bf,0x2bc,0x2b9,0x2b6,0x2b3,0x2b0,0x2ad,0x2ab,0x2a8,0x2a5,0x2a2,
		0x29f,0x29c,0x299,0x296,0x293,0x290,0x28d,0x28a,0x287,0x284,0x280,0x27d,0x27a,0x277,0x274,0x271,0x26e,0x26b,0x268,0x265,
		0x262,0x25f,0x25c,0x259,0x255,0x252,0x24f,0x24c,0x249,0x246,0x243,0x240,0x23d,0x239,0x236,0x233,0x230,0x22d,0x22a,0x227,
		0x224,0x220,0x21d,0x21a,0x217,0x214,0x211,0x20e,0x20a,0x207,0x204,0x201,0x1fe,0x1fb,0x1f8,0x1f5,0x1f1,0x1ee,0x1eb,0x1e8,
		0x1e5,0x1e2,0x1df,0x1db,0x1d8,0x1d5,0x1d2,0x1cf,0x1cc,0x1c9,0x1c6,0x1c2,0x1bf,0x1bc,0x1b9,0x1b6,0x1b3,0x1b0,0x1ad,0x1aa,
		0x1a6,0x1a3,0x1a0,0x19d,0x19a,0x197,0x194,0x191,0x18e,0x18b,0x188,0x185,0x182,0x17f,0x17b,0x178,0x175,0x172,0x16f,0x16c,
		0x169,0x166,0x163,0x160,0x15d,0x15a,0x157,0x154,0x152,0x14f,0x14c,0x149,0x146,0x143,0x140,0x13d,0x13a,0x137,0x134,0x131,
		0x12f,0x12c,0x129,0x126,0x123,0x120,0x11e,0x11b,0x118,0x115,0x112,0x110,0x10d,0x10a,0x107,0x105,0x102,0xff,0xfc,0xfa,
		0xf7,0xf4,0xf2,0xef,0xec,0xea,0xe7,0xe4,0xe2,0xdf,0xdd,0xda,0xd7,0xd5,0xd2,0xd0,0xcd,0xcb,0xc8,0xc6,
		0xc3,0xc1,0xbe,0xbc,0xba,0xb7,0xb5,0xb2,0xb0,0xae,0xab,0xa9,0xa7,0xa4,0xa2,0xa0,0x9d,0x9b,0x99,0x97,
		0x94,0x92,0x90,0x8e,0x8c,0x8a,0x87,0x85,0x83,0x81,0x7f,0x7d,0x7b,0x79,0x77,0x75,0x73,0x71,0x6f,0x6d,
		0x6b,0x69,0x67,0x65,0x63,0x62,0x60,0x5e,0x5c,0x5a,0x59,0x57,0x55,0x53,0x52,0x50,0x4e,0x4d,0x4b,0x49,
		0x48,0x46,0x45,0x43,0x41,0x40,0x3e,0x3d,0x3b,0x3a,0x39,0x37,0x36,0x34,0x33,0x32,0x30,0x2f,0x2e,0x2c,
		0x2b,0x2a,0x29,0x27,0x26,0x25,0x24,0x23,0x21,0x20,0x1f,0x1e,0x1d,0x1c,0x1b,0x1a,0x19,0x18,0x17,0x16,
		0x15,0x14,0x14,0x13,0x12,0x11,0x10,0x10,0xf,0xe,0xd,0xd,0xc,0xb,0xb,0xa,0x9,0x9,0x8,0x8,
		0x7,0x7,0x6,0x6,0x5,0x5,0x4,0x4,0x4,0x3,0x3,0x3,0x2,0x2,0x2,0x1,0x1,0x1,0x1,0x1,
		0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x1,0x1,0x1,0x1,
		0x2,0x2,0x2,0x2,0x3,0x3,0x3,0x4,0x4,0x5,0x5,0x5,0x6,0x6,0x7,0x7,0x8,0x9,0x9,0xa,
		0xa,0xb,0xc,0xc,0xd,0xe,0xe,0xf,0x10,0x11,0x12,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,
		0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x26,0x27,0x28,0x29,0x2a,0x2c,0x2d,0x2e,0x30,0x31,
		0x32,0x34,0x35,0x36,0x38,0x39,0x3b,0x3c,0x3e,0x3f,0x41,0x42,0x44,0x45,0x47,0x49,0x4a,0x4c,0x4d,0x4f,
		0x51,0x52,0x54,0x56,0x58,0x59,0x5b,0x5d,0x5f,0x61,0x63,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x72,0x74,
		0x76,0x78,0x7a,0x7c,0x7e,0x80,0x82,0x84,0x86,0x88,0x8b,0x8d,0x8f,0x91,0x93,0x96,0x98,0x9a,0x9c,0x9f,
		0xa1,0xa3,0xa5,0xa8,0xaa,0xac,0xaf,0xb1,0xb4,0xb6,0xb8,0xbb,0xbd,0xc0,0xc2,0xc5,0xc7,0xca,0xcc,0xcf,
		0xd1,0xd4,0xd6,0xd9,0xdb,0xde,0xe0,0xe3,0xe6,0xe8,0xeb,0xee,0xf0,0xf3,0xf6,0xf8,0xfb,0xfe,0x100,0x103,
		0x106,0x109,0x10b,0x10e,0x111,0x114,0x117,0x119,0x11c,0x11f,0x122,0x125,0x127,0x12a,0x12d,0x130,0x133,0x136,0x139,0x13c,
		0x13e,0x141,0x144,0x147,0x14a,0x14d,0x150,0x153,0x156,0x159,0x15c,0x15f,0x162,0x165,0x168,0x16b,0x16e,0x171,0x174,0x177,
		0x17a,0x17d,0x180,0x183,0x186,0x189,0x18c,0x18f,0x192,0x195,0x199,0x19c,0x19f,0x1a2,0x1a5,0x1a8,0x1ab,0x1ae,0x1b1,0x1b4,
		0x1b7,0x1bb,0x1be,0x1c1,0x1c4,0x1c7,0x1ca,0x1cd,0x1d0,0x1d4,0x1d7,0x1da,0x1dd,0x1e0,0x1e3,0x1e6,0x1ea,0x1ed,0x1f0,0x1f3,
		0x1f6,0x1f9,0x1fc,0x200
};

const uint16_t triang [1024] =
{
		0x2,0x4,0x6,0x8,0xa,0xc,0xe,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e,0x20,0x22,0x24,0x26,0x28,
		0x2a,0x2c,0x2e,0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e,0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e,0x50,
		0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e,0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x72,0x74,0x76,0x78,
		0x7a,0x7c,0x7e,0x80,0x82,0x84,0x86,0x88,0x8a,0x8c,0x8e,0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e,0xa0,
		0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae,0xb0,0xb2,0xb4,0xb6,0xb8,0xba,0xbc,0xbe,0xc0,0xc2,0xc4,0xc6,0xc8,
		0xca,0xcc,0xce,0xd0,0xd2,0xd4,0xd6,0xd8,0xda,0xdc,0xde,0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xee,0xf0,
		0xf2,0xf4,0xf6,0xf8,0xfa,0xfc,0xfe,0x100,0x102,0x104,0x106,0x108,0x10a,0x10c,0x10e,0x110,0x112,0x114,0x116,0x118,
		0x11a,0x11c,0x11e,0x120,0x122,0x124,0x126,0x128,0x12a,0x12c,0x12e,0x130,0x132,0x134,0x136,0x138,0x13a,0x13c,0x13e,0x140,
		0x142,0x144,0x146,0x148,0x14a,0x14c,0x14e,0x150,0x152,0x154,0x156,0x158,0x15a,0x15c,0x15e,0x160,0x162,0x164,0x166,0x168,
		0x16a,0x16c,0x16e,0x170,0x172,0x174,0x176,0x178,0x17a,0x17c,0x17e,0x180,0x182,0x184,0x186,0x188,0x18a,0x18c,0x18e,0x190,
		0x192,0x194,0x196,0x198,0x19a,0x19c,0x19e,0x1a0,0x1a2,0x1a4,0x1a6,0x1a8,0x1aa,0x1ac,0x1ae,0x1b0,0x1b2,0x1b4,0x1b6,0x1b8,
		0x1ba,0x1bc,0x1be,0x1c0,0x1c2,0x1c4,0x1c6,0x1c8,0x1ca,0x1cc,0x1ce,0x1d0,0x1d2,0x1d4,0x1d6,0x1d8,0x1da,0x1dc,0x1de,0x1e0,
		0x1e2,0x1e4,0x1e6,0x1e8,0x1ea,0x1ec,0x1ee,0x1f0,0x1f2,0x1f4,0x1f6,0x1f8,0x1fa,0x1fc,0x1fe,0x200,0x202,0x204,0x206,0x208,
		0x20a,0x20c,0x20e,0x210,0x212,0x214,0x216,0x218,0x21a,0x21c,0x21e,0x220,0x222,0x224,0x226,0x228,0x22a,0x22c,0x22e,0x230,
		0x232,0x234,0x236,0x238,0x23a,0x23c,0x23e,0x240,0x242,0x244,0x246,0x248,0x24a,0x24c,0x24e,0x250,0x252,0x254,0x256,0x258,
		0x25a,0x25c,0x25e,0x260,0x262,0x264,0x266,0x268,0x26a,0x26c,0x26e,0x270,0x272,0x274,0x276,0x278,0x27a,0x27c,0x27e,0x280,
		0x282,0x284,0x286,0x288,0x28a,0x28c,0x28e,0x290,0x292,0x294,0x296,0x298,0x29a,0x29c,0x29e,0x2a0,0x2a2,0x2a4,0x2a6,0x2a8,
		0x2aa,0x2ac,0x2ae,0x2b0,0x2b2,0x2b4,0x2b6,0x2b8,0x2ba,0x2bc,0x2be,0x2c0,0x2c2,0x2c4,0x2c6,0x2c8,0x2ca,0x2cc,0x2ce,0x2d0,
		0x2d2,0x2d4,0x2d6,0x2d8,0x2da,0x2dc,0x2de,0x2e0,0x2e2,0x2e4,0x2e6,0x2e8,0x2ea,0x2ec,0x2ee,0x2f0,0x2f2,0x2f4,0x2f6,0x2f8,
		0x2fa,0x2fc,0x2fe,0x300,0x302,0x304,0x306,0x308,0x30a,0x30c,0x30e,0x310,0x312,0x314,0x316,0x318,0x31a,0x31c,0x31e,0x320,
		0x322,0x324,0x326,0x328,0x32a,0x32c,0x32e,0x330,0x332,0x334,0x336,0x338,0x33a,0x33c,0x33e,0x340,0x342,0x344,0x346,0x348,
		0x34a,0x34c,0x34e,0x350,0x352,0x354,0x356,0x358,0x35a,0x35c,0x35e,0x360,0x362,0x364,0x366,0x368,0x36a,0x36c,0x36e,0x370,
		0x372,0x374,0x376,0x378,0x37a,0x37c,0x37e,0x380,0x382,0x384,0x386,0x388,0x38a,0x38c,0x38e,0x390,0x392,0x394,0x396,0x398,
		0x39a,0x39c,0x39e,0x3a0,0x3a2,0x3a4,0x3a6,0x3a8,0x3aa,0x3ac,0x3ae,0x3b0,0x3b2,0x3b4,0x3b6,0x3b8,0x3ba,0x3bc,0x3be,0x3c0,
		0x3c2,0x3c4,0x3c6,0x3c8,0x3ca,0x3cc,0x3ce,0x3d0,0x3d2,0x3d4,0x3d6,0x3d8,0x3da,0x3dc,0x3de,0x3e0,0x3e2,0x3e4,0x3e6,0x3e8,
		0x3ea,0x3ec,0x3ee,0x3f0,0x3f2,0x3f4,0x3f6,0x3f8,0x3fa,0x3fc,0x3fe,0x3ff,0x3fe,0x3fc,0x3fa,0x3f8,0x3f6,0x3f4,0x3f2,0x3f0,
		0x3ee,0x3ec,0x3ea,0x3e8,0x3e6,0x3e4,0x3e2,0x3e0,0x3de,0x3dc,0x3da,0x3d8,0x3d6,0x3d4,0x3d2,0x3d0,0x3ce,0x3cc,0x3ca,0x3c8,
		0x3c6,0x3c4,0x3c2,0x3c0,0x3be,0x3bc,0x3ba,0x3b8,0x3b6,0x3b4,0x3b2,0x3b0,0x3ae,0x3ac,0x3aa,0x3a8,0x3a6,0x3a4,0x3a2,0x3a0,
		0x39e,0x39c,0x39a,0x398,0x396,0x394,0x392,0x390,0x38e,0x38c,0x38a,0x388,0x386,0x384,0x382,0x380,0x37e,0x37c,0x37a,0x378,
		0x376,0x374,0x372,0x370,0x36e,0x36c,0x36a,0x368,0x366,0x364,0x362,0x360,0x35e,0x35c,0x35a,0x358,0x356,0x354,0x352,0x350,
		0x34e,0x34c,0x34a,0x348,0x346,0x344,0x342,0x340,0x33e,0x33c,0x33a,0x338,0x336,0x334,0x332,0x330,0x32e,0x32c,0x32a,0x328,
		0x326,0x324,0x322,0x320,0x31e,0x31c,0x31a,0x318,0x316,0x314,0x312,0x310,0x30e,0x30c,0x30a,0x308,0x306,0x304,0x302,0x300,
		0x2fe,0x2fc,0x2fa,0x2f8,0x2f6,0x2f4,0x2f2,0x2f0,0x2ee,0x2ec,0x2ea,0x2e8,0x2e6,0x2e4,0x2e2,0x2e0,0x2de,0x2dc,0x2da,0x2d8,
		0x2d6,0x2d4,0x2d2,0x2d0,0x2ce,0x2cc,0x2ca,0x2c8,0x2c6,0x2c4,0x2c2,0x2c0,0x2be,0x2bc,0x2ba,0x2b8,0x2b6,0x2b4,0x2b2,0x2b0,
		0x2ae,0x2ac,0x2aa,0x2a8,0x2a6,0x2a4,0x2a2,0x2a0,0x29e,0x29c,0x29a,0x298,0x296,0x294,0x292,0x290,0x28e,0x28c,0x28a,0x288,
		0x286,0x284,0x282,0x280,0x27e,0x27c,0x27a,0x278,0x276,0x274,0x272,0x270,0x26e,0x26c,0x26a,0x268,0x266,0x264,0x262,0x260,
		0x25e,0x25c,0x25a,0x258,0x256,0x254,0x252,0x250,0x24e,0x24c,0x24a,0x248,0x246,0x244,0x242,0x240,0x23e,0x23c,0x23a,0x238,
		0x236,0x234,0x232,0x230,0x22e,0x22c,0x22a,0x228,0x226,0x224,0x222,0x220,0x21e,0x21c,0x21a,0x218,0x216,0x214,0x212,0x210,
		0x20e,0x20c,0x20a,0x208,0x206,0x204,0x202,0x200,0x1fe,0x1fc,0x1fa,0x1f8,0x1f6,0x1f4,0x1f2,0x1f0,0x1ee,0x1ec,0x1ea,0x1e8,
		0x1e6,0x1e4,0x1e2,0x1e0,0x1de,0x1dc,0x1da,0x1d8,0x1d6,0x1d4,0x1d2,0x1d0,0x1ce,0x1cc,0x1ca,0x1c8,0x1c6,0x1c4,0x1c2,0x1c0,
		0x1be,0x1bc,0x1ba,0x1b8,0x1b6,0x1b4,0x1b2,0x1b0,0x1ae,0x1ac,0x1aa,0x1a8,0x1a6,0x1a4,0x1a2,0x1a0,0x19e,0x19c,0x19a,0x198,
		0x196,0x194,0x192,0x190,0x18e,0x18c,0x18a,0x188,0x186,0x184,0x182,0x180,0x17e,0x17c,0x17a,0x178,0x176,0x174,0x172,0x170,
		0x16e,0x16c,0x16a,0x168,0x166,0x164,0x162,0x160,0x15e,0x15c,0x15a,0x158,0x156,0x154,0x152,0x150,0x14e,0x14c,0x14a,0x148,
		0x146,0x144,0x142,0x140,0x13e,0x13c,0x13a,0x138,0x136,0x134,0x132,0x130,0x12e,0x12c,0x12a,0x128,0x126,0x124,0x122,0x120,
		0x11e,0x11c,0x11a,0x118,0x116,0x114,0x112,0x110,0x10e,0x10c,0x10a,0x108,0x106,0x104,0x102,0x100,0xfe,0xfc,0xfa,0xf8,
		0xf6,0xf4,0xf2,0xf0,0xee,0xec,0xea,0xe8,0xe6,0xe4,0xe2,0xe0,0xde,0xdc,0xda,0xd8,0xd6,0xd4,0xd2,0xd0,
		0xce,0xcc,0xca,0xc8,0xc6,0xc4,0xc2,0xc0,0xbe,0xbc,0xba,0xb8,0xb6,0xb4,0xb2,0xb0,0xae,0xac,0xaa,0xa8,
		0xa6,0xa4,0xa2,0xa0,0x9e,0x9c,0x9a,0x98,0x96,0x94,0x92,0x90,0x8e,0x8c,0x8a,0x88,0x86,0x84,0x82,0x80,
		0x7e,0x7c,0x7a,0x78,0x76,0x74,0x72,0x70,0x6e,0x6c,0x6a,0x68,0x66,0x64,0x62,0x60,0x5e,0x5c,0x5a,0x58,
		0x56,0x54,0x52,0x50,0x4e,0x4c,0x4a,0x48,0x46,0x44,0x42,0x40,0x3e,0x3c,0x3a,0x38,0x36,0x34,0x32,0x30,
		0x2e,0x2c,0x2a,0x28,0x26,0x24,0x22,0x20,0x1e,0x1c,0x1a,0x18,0x16,0x14,0x12,0x10,0xe,0xc,0xa,0x8,
		0x6,0x4,0x2,0x0,};

int main(void)
{

	SystemInit()	;
	configPins()	;
	configADC()		;
	configDAC()		;
	LPC_GPIO0->FIODIR |= (1<<22);
	LPC_GPIO0->FIOCLR  = (1<<22);



    while(1)
    {

    	switch(filtro)
		{
			case 0:	/*	No hay efecto*/

				DAC_UpdateValue(LPC_DAC, ADC0Value);

				break;

			case 1: /*	Efecto bit_crusher*/

				DAC_UpdateValue(LPC_DAC, ((ADC0Value<<bit_crush)&0x03FF));

				break;

			case 2: /*	Efecto de distorsion*/

				if(ADC0Value > 512+distortion)
					ADC0Value = 512+distortion		;	//Le agregue que la distorsión tambien se aplique en los minimos del sine.
				else if(ADC0Value < 512-distortion)
					ADC0Value = 512-distortion		;
				DAC_UpdateValue(LPC_DAC, ADC0Value)	;

				break;

			case 3: /*	Efecto de fuzz*/

				if(ADC0Value > (fuzz+511))
					ADC0Value = 1023;
				else if(ADC0Value < (511-fuzz))
					ADC0Value = 0	;

				DAC_UpdateValue(LPC_DAC, ADC0Value);

				break;

			case 4: /*	Efecto de modulacion seno*/

				divider++;
				if(divider == 4)
				{
					divider = 0				;
					sample	= sample + speed;
				}
				if(sample > 1023)
					sample	= 0				;

				ADC0Value = map(ADC0Value, 0, 1023, 0, seno[sample]);
				DAC_UpdateValue(LPC_DAC, ADC0Value)					;

				break;

			case 5: /*	Efecto de modulacion triangulo*/

				divider++;
				if(divider == 4)
				{
					divider = 0				;
					sample	= sample + speed;
				}
				if(sample > 1023)
					sample	= 0				;

				ADC0Value = map(ADC0Value, 0, 1023, 0, triang[sample])	;
				DAC_UpdateValue(LPC_DAC, ADC0Value)						;

				break;

			default:
				break;
		}
    }

    return 0 ;
}

void configPins()
{	/*	Configuro para que esten activos los pines AD0.0, AOUT y EINT1,2,3*/

	PINSEL_CFG_Type 	pin_cfg					;
	pin_cfg.OpenDrain = PINSEL_PINMODE_NORMAL	;
	pin_cfg.Pinmode   = PINSEL_PINMODE_TRISTATE ;
	pin_cfg.Portnum	  = PINSEL_PORT_0			;
	pin_cfg.Funcnum   = PINSEL_FUNC_1			;
	pin_cfg.Pinnum	  = PINSEL_PIN_23			;

	PINSEL_ConfigPin(&pin_cfg)					;		//AD0.0

	pin_cfg.Pinnum	  = PINSEL_PIN_26			;
	pin_cfg.Funcnum   = PINSEL_FUNC_2			;
	pin_cfg.Pinmode   = PINSEL_PINMODE_PULLUP	;

	PINSEL_ConfigPin(&pin_cfg)					;		//AOUT

	pin_cfg.Funcnum   = PINSEL_FUNC_1			;
	pin_cfg.Pinmode   = PINSEL_PINMODE_PULLUP 	;
	pin_cfg.Portnum	  = PINSEL_PORT_2			;
	pin_cfg.Pinnum	  = PINSEL_PIN_11			;

	PINSEL_ConfigPin(&pin_cfg)					;		//EINT1

	pin_cfg.Pinnum	  = PINSEL_PIN_12			;

	PINSEL_ConfigPin(&pin_cfg)					;		//EINT2

	pin_cfg.Pinnum	  = PINSEL_PIN_13			;

	PINSEL_ConfigPin(&pin_cfg)					;		//EINT3

	NVIC_DisableIRQ(EINT1_IRQn)					;		/* Para configurar EINTx*/
	NVIC_DisableIRQ(EINT2_IRQn)					;
	NVIC_DisableIRQ(EINT3_IRQn)					;

	EXTI_Init()									;

	EXTI_InitTypeDef exticfg					;
	exticfg.EXTI_Line = EXTI_EINT1				;
	exticfg.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
	exticfg.EXTI_polarity = EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;

	EXTI_Config(&exticfg) 						;		/* Configuro EINT1 = seleccionar filtro*/

	exticfg.EXTI_Line = EXTI_EINT2				;

	EXTI_Config(&exticfg) 						;		/* Configuro EINT2 = aumento de variable de distorsion*/

	exticfg.EXTI_Line = EXTI_EINT3				;

	EXTI_Config(&exticfg) 						;		/* Configuro EINT3 = decremento de variable de distorsion*/

	EXTI_ClearEXTIFlag(EXTI_EINT1)				;		/*	Por las dudas*/
	EXTI_ClearEXTIFlag(EXTI_EINT2)				;
	EXTI_ClearEXTIFlag(EXTI_EINT3)				;

	NVIC_SetPriority(EINT1_IRQn, 0) 			;		/*	Tienen mas prioridad que el ADC*/
	NVIC_SetPriority(EINT2_IRQn, 1) 			;
	NVIC_SetPriority(EINT2_IRQn, 2) 			;

	NVIC_EnableIRQ(EINT1_IRQn)					;
	NVIC_EnableIRQ(EINT2_IRQn)					;
	NVIC_EnableIRQ(EINT3_IRQn)					;

	return;
}

void configDAC()
{	/* Se trabaja a menor frecuencia (max 400[kHz]) porque no tiene sentido trabajar más rapido que
 	   el ADC. Se aprovecha para ahorrar en consumo.
 	*/

	DAC_Init(LPC_DAC)		;
	DAC_SetBias(LPC_DAC, 1)	;	/* Se setea menor corriente*/

	return;
}

void configADC()
{	/* Configuro ADC y el canal 0 para que funcione a la max. vel. posible y en modo BURST */

	ADC_Init(LPC_ADC, 200000) 					;
	ADC_BurstCmd(LPC_ADC, SET) 					;	//Activo modo BURST
	ADC_ChannelCmd(LPC_ADC, CHANNEL_0, ENABLE)	;	//Activo un canal
//	ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, SET)	;	//Activa interr. por ADC.0
	LPC_ADC->ADINTEN = 1						;	//Activa interr. por ADC.0

	NVIC_SetPriority(ADC_IRQn, 3) 				;
	NVIC_EnableIRQ(ADC_IRQn) 					;

	return;
}

void ADC_IRQHandler()
{
	//Leo los valores y enmascaro para leer los 12 LSB (el ADC es de 12 bits)
	ADC0Value = (ADC_GetData(CHANNEL_0)>>2)	;
	
	return;
}

void EINT1_IRQHandler()
{
	EXTI_ClearEXTIFlag(EXTI_EINT1)	;

	/*	En un principio hay 5 filtros nomas
	 * 	1) Sin filtro
	 * 	2) Filtro bit-crusher
	 * 	3) Distorsion
	 * 	4) Fuzz
	 * 	5) Modulacion seno
	 * 	6) Modulacion triangulo
	 * */

	filtro++  		;

	if(filtro >5)
		filtro = 0	;

	return;
}

void EINT2_IRQHandler()
{
	EXTI_ClearEXTIFlag(EXTI_EINT2)	;

	/*	Aumento la variable correspondiente al filtro que se esta utilizando*/

	switch(filtro)
	{
		case 0:	/*	Sin filtro	VER SI SE PUEDE IMPLEMENTAR ALGO*/

			break;

		case 1:	/*	Filtro bit crusher*/

			if(bit_crush<10)
				bit_crush ++;

			break;

		case 2:	/*	Filtro distorsion*/

			if(distortion <= 450)
				distortion += 50;

			break;

		case 3:	/*	Filtro fuzz*/

			if(fuzz < 511)
				fuzz += 25;

			break;

		case 4:	/*	Filtro modulacion seno*/

			if (speed < 1023)
				speed =	speed + 1;

			break;

		case 5:	/*	Filtro modulacion triangulo*/

			if (speed < 1023)
				speed =	speed + 1;

			break;

		default:
			break;
	}

	return;
}

void EINT3_IRQHandler()
{
	EXTI_ClearEXTIFlag(EXTI_EINT3)	;

	/*	Decremento la variable correspondiente al filtro que se esta utilizando*/

	switch(filtro)
	{
		case(0):	/*	Sin filtro	VER SI SE PUEDE IMPLEMENTAR ALGO*/

			break;

		case(1):	/*	Filtro bit crusher*/

			if(bit_crush>0)
				bit_crush = bit_crush-1;

			break;

		case 2:	/*	Filtro distorsion*/

			if(distortion > 200)
				distortion -= 50;

			break;

		case 3:	/*	Filtro fuzz*/

			if(fuzz > 0)
				fuzz = fuzz - 25;

			break;

		case 4:	/*	Filtro modulacion seno*/

			if (speed > 0)
				speed =	speed - 1;

			break;

		case 5:	/*	Filtro modulacion triangulo*/

			if (speed > 0)
				speed =	speed - 1;

			break;

		default:

			break;
	}

	return;
}

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

