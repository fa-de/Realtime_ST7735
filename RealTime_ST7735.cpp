#include "Realtime_ST7735.h"

#include "tileset.h"

Realtime_ST7735::Realtime_ST7735(uint8_t cs, uint8_t rs, uint8_t rst = -1) : Adafruit_ST7735(cs, rs, rst), bgcolor(Color(0, 0, 0))
{
	//Unfortunately, Adafruit have made rsport, csport and their masks private instead of protected, so we have to save them again
	csport = portOutputRegister(digitalPinToPort(cs));
	rsport = portOutputRegister(digitalPinToPort(rs));
	cspinmask = digitalPinToBitMask(cs);
	rspinmask = digitalPinToBitMask(rs);

	for (int i = 0; i < NSPRITES; i++)
	{
		previousSprite[i].active = false;
		sprite[i].active = false;
	}

	for (int t = 0; t < NTILES; t++)
	{
		tile[t].dirty = true;
	}
}

//Draw background and all sprites into tile buffer
//Returns:	true if redraw is required
//			false else
//Parameters:
//			data	tile buffer
//			x0, y0	Tile offset
//			t		tile index
void Realtime_ST7735::render_tile(uint16_t* buffer, int16_t x0, int16_t y0, uint16_t t)
{
	//Fill sprite with background color
	for (int i = 0; i < TILE_WIDTH * TILE_HEIGHT; i++) buffer[i] = bgcolor;

	//Draw all touching and active sprites into tile
	for (int i = 0; i < NSPRITES; i++)
	{
		const Sprite& sprite = this->sprite[i];

		int deltax = (int)sprite.x - x0;
		int deltay = (int)sprite.y - y0;

		//TODO: Optimize
		if (!sprite.active ||
			sprite.x < x0 - SPRITE_WIDTH || sprite.x >= x0 + TILE_WIDTH ||
			sprite.y < y0 - SPRITE_HEIGHT || sprite.y >= y0 + TILE_HEIGHT)
		{
			continue;
		}

		int srcx_lo = max(-deltax, 0);
		int srcx_hi = min(TILE_WIDTH - deltax, SPRITE_WIDTH);

		int dstx_lo = max(deltax, 0);
		int dstx_hi = min(TILE_WIDTH, deltax + SPRITE_WIDTH);

		int w = min(srcx_hi - srcx_lo, dstx_hi - dstx_lo);


		int srcy_lo = max(-deltay, 0);
		int srcy_hi = min(TILE_HEIGHT - deltay, SPRITE_HEIGHT);

		int dsty_lo = max(deltay, 0);
		int dsty_hi = min(TILE_HEIGHT, deltay + SPRITE_HEIGHT);

		int h = min(srcy_hi - srcy_lo, dsty_hi - dsty_lo);

		for (int y = 0; y < h; y++)
		{
			int srcy = srcy_lo + y;
			int dsty = dsty_lo + y;
			memcpy_P(&buffer[dsty*TILE_WIDTH + dstx_lo], &TILESET[sprite.sprite][srcy*SPRITE_WIDTH + srcx_lo], w * 2);
		}
	}
}

void Realtime_ST7735::upload_tile(const color_t *buffer, uint16_t x, uint16_t y)
{
	setAddrWindow(x, y, x + TILE_WIDTH - 1, y + TILE_HEIGHT - 1);

	*rsport |= rspinmask;
	*csport &= ~cspinmask;

	SPI.transfer(buffer, TILE_WIDTH * TILE_HEIGHT * sizeof(*buffer));

	*csport |= cspinmask;
}

static inline unsigned int pos_to_tile_id(Realtime_ST7735::pos_t x, Realtime_ST7735::pos_t y)
{
	const int X = x / TILE_WIDTH;
	const int Y = y / TILE_HEIGHT;
	const int tile = Y * (SCREEN_WIDTH / TILE_WIDTH) + X;
	return min(max(tile, 0), NTILES - 1);
}

void Realtime_ST7735::invalidate_touching_tiles(const struct Sprite& sprite)
{	
	if (!sprite.active) return;

	//A sprite can touch at most 4 tiles at the same time if
	//SPRITE_WIDTH <= TILE_WIDTH and SPRITE_HEIGHT <= TILE_HEIGHT, which we enforce

	//UL corner
	tile[pos_to_tile_id(sprite.x, sprite.y)].dirty = true;
	//UR
	tile[pos_to_tile_id(sprite.x + SPRITE_WIDTH - 1, sprite.y)].dirty = true;
	//LL
	tile[pos_to_tile_id(sprite.x, sprite.y + SPRITE_HEIGHT - 1)].dirty = true;
	//LR
	tile[pos_to_tile_id(sprite.x + SPRITE_WIDTH - 1, sprite.y + SPRITE_HEIGHT - 1)].dirty = true;
}

void Realtime_ST7735::render_sprites()
{
	for (int i = 0; i < NSPRITES; i++)
	{
		//TODO: Only update if the sprite has changed
		if (sprite[i] == previousSprite[i]) continue;

		//First, invalidate the previously touched tiles:
		invalidate_touching_tiles(previousSprite[i]);

		//Then invalidate the tiles it touches now:
		invalidate_touching_tiles(sprite[i]);

		//Save current state
		previousSprite[i] = sprite[i];
	}

	color_t buffer[TILE_WIDTH * TILE_HEIGHT];

	int t = 0;
	for (int y = 0; y < 128; y += TILE_HEIGHT)
	{
		for (int x = 0; x < 160; x += TILE_WIDTH)
		{
			if(tile[t].dirty)
			{
				render_tile(buffer, x, y, t);
				upload_tile(buffer, x, y);
				tile[t].dirty = false;
			}
			else
			{
				//Only useful for debugging:
				//colorfill_tile(0xfab1, x, y);
			}
			t++;
		}
	}
}

//Clear tile with a single color
void Realtime_ST7735::colorfill_tile(uint16_t color, uint16_t x, uint16_t y)
{
	setAddrWindow(x, y, x + TILE_WIDTH - 1, y + TILE_HEIGHT - 1);

	*rsport |= rspinmask;
	*csport &= ~cspinmask;

	for (int i = 0; i < TILE_WIDTH * TILE_HEIGHT; i++) SPI.transfer16(color);

	*csport |= cspinmask;
}

//********** Text rendering **********

#include <glcdfont.c>	//Requires Adafruit_GFX_library

static void CharToBuffer(int16_t* b, unsigned char c)
{
	uint8_t glyph[5];

	memcpy_P(glyph, font + (c * 5), 5);

	int16_t *p = b;

	for (int y = 0; y<7; y++)
	{
		for (int x = 0; x<5; x++)
		{
			bool set = glyph[x] & (1 << y);
			*p++ = set ? 0xffff : 0x0000;
		}
	}
}

void Realtime_ST7735::fastprintChar(int row, int col, unsigned char ch)
{
	constexpr size_t npixels = 5 * 7;
	size_t buffersize = npixels * sizeof(int16_t);
	int16_t buffer[npixels];

	//DEBUG
	CharToBuffer(buffer, ch);

	int16_t x = 8 + col * 6;
	int16_t y = 8 + row * 11;

	setAddrWindow(x, y, x + 5 - 1, y + 7 - 1);

	*rsport |= rspinmask;
	*csport &= ~cspinmask;

	SPI.transfer(buffer, buffersize);

	*csport |= cspinmask;
}
