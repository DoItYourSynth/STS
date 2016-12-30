enum Palette {

	OFF,
	WHITE,
	RED,
	GREEN,
	BLUE,
	YELLOW,
	CYAN,
	ORANGE,
	VIOLET,
	NUM_LED_PALETTE

};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF
		{500,	500,	350},	//WHITE
		{1000,	0,		0},		//RED
		{0,		500,	0},		//GREEN
		{0,		0,		800},	//BLUE
		{600,	400,	0},		//YELLOW
		{0,		760,	1000},	//CYAN
		{1000,	200,	0},		//ORANGE
		{1000,	0,		1000}	//VIOLET

};

