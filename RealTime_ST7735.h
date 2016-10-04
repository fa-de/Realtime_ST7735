#ifndef _REALTIME_ST3375_h
#define _REALTIME_ST3375_h

#include <SPI.h>
#include <avr/pgmspace.h>
#include <Adafruit_ST7735.h>

class Realtime_ST7735 : public Adafruit_ST7735
{
//********** The sprite engine **********
//Configuration
	#define SPRITE_WIDTH 8
	#define SPRITE_HEIGHT 8

	#define NSPRITES 8			//The maximum number of concurrently active sprites
								//Lower this to save some memory (~12 byte per sprite) and a tiny bit of computational load

	#define SCREEN_WIDTH 160	//Width...
	#define SCREEN_HEIGHT 128	//...and height of the screen
	#define TILE_WIDTH 16		//Size of screen tiles
	#define TILE_HEIGHT 16		//		  ''
	#define NTILES (SCREEN_WIDTH/TILE_WIDTH)*(SCREEN_HEIGHT/TILE_HEIGHT)	//Number of tiles in total

	//Check configuration
	static_assert(TILE_WIDTH >= SPRITE_WIDTH, "Please ensure that sprites are no larger than tiles. This simplifies overlap detection a lot");
	static_assert(TILE_HEIGHT >= SPRITE_HEIGHT, "Please ensure that sprites are no larger than tiles. This simplifies overlap detection a lot");
	static_assert(SCREEN_WIDTH % TILE_WIDTH == 0, "Tile dimensions must be an integer divider of the screen size");
	static_assert(SCREEN_HEIGHT % TILE_HEIGHT == 0, "Tile dimensions must be an integer divider of the screen size");

public:
	//Internal types:
	typedef uint16_t color_t;
	typedef int16_t pos_t;

	//Public properties:
	color_t bgcolor;	// The background color
	struct Sprite
	{
		bool active;	// should this sprite be drawn in the next render_sprites() step?
		pos_t x;		// x position in pixels
		pos_t y;		// y position in pixels
		uint8_t sprite;	//Index into the sprite map
		
		bool operator == (const Sprite& rhs) const
		{
			return (active == rhs.active && x == rhs.x && y == rhs.y && sprite == rhs.sprite);
		}
	} sprite[NSPRITES];

	//Public functions:
	Realtime_ST7735(uint8_t cs, uint8_t rs, uint8_t rst = -1);
	void render_sprites();	//Draws the screen

	//Returns a byte-swapped 5/6/5-bit R/G/B color for storage in an int16_t array
	static inline color_t Color(uint8_t r, uint8_t g, uint8_t b) {
		static_assert(sizeof(color_t) == 2, "Color type is not 16 bit wide");

		//Convert to MSB
		color_t lsb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
		return (lsb >> 8) | (lsb << 8);
	}
	
private:

	//The previous state of all sprites
	struct Sprite previousSprite[NSPRITES];

	struct Tile
	{
		bool dirty;
	} tile[NTILES];

	void invalidate_touching_tiles(const struct Sprite& sprite);
	void render_tile(uint16_t* buffer, int16_t x0, int16_t y0, uint16_t t);
	void upload_tile(const uint16_t *buffer, uint16_t x, uint16_t y);
	void colorfill_tile(color_t color, uint16_t x0, uint16_t y0);
	
	volatile uint8_t *csport, *rsport;
	uint8_t cspinmask, rspinmask;

//********** Faster text rendering **********

public:
	void fastprintChar(int row, int col, unsigned char ch);
};

#endif
