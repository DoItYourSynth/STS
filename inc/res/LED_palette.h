#ifndef LED_PALETTE_H_
#define LED_PALETTE_H_

enum Palette{
	OFF,
	
	WHITE,	//1
	RED, 	//2
	ORANGE,	//3
	YELLOW,	//4
	GREEN,	//5
	CYAN,	//6
	BLUE,	//7
	VIOLET,	//8
	LAVENDER,//9
	PINK,	//10

	AQUA,
	GREENER,
	CYANER,
	DIM_WHITE,
	DIM_RED,

	NUM_LED_PALETTE
};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF

		{930,	950,	900},	//WHITE
		{1000,	0,		0},		//RED
		{1000,	200,	0},		//ORANGE
		{600,	500,	0},		//YELLOW
		{0,		600,	60},	//GREEN
		{0,		560,	1000},	//CYAN (SKY BLUE)
		{0,		0,		1000},	//BLUE
		{1000,	0,		1000},	//VIOLET (HOT PINK)
		{410,   0,      1000}, 	//LAVENDER
		{520,	180,	170},	//PINK (PEACH)

		{0, 	280, 	150},  	//AQUA
		{550, 	700, 	0}, 	//GREENER
		{200, 	800, 	1000}, 	//CYANER
		{100, 	100, 	100}, 	//DIM_WHITE
		{50, 	0, 		0} 		//DIM_RED
};

#endif
