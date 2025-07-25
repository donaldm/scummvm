/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/substream.h"
#include "graphics/macgui/macwindowmanager.h"
#include "graphics/pixelformat.h"
#include "image/codecs/bmp_raw.h"

#include "director/director.h"
#include "director/images.h"

namespace Director {

DIBDecoder::DIBDecoder() : _palette(0) {
	_surface = nullptr;
	_bitsPerPixel = 0;
	_codec = nullptr;
}

DIBDecoder::~DIBDecoder() {
	destroy();
}

void DIBDecoder::destroy() {
	_surface = nullptr;	// It is deleted by BitmapRawDecoder

	_palette.clear();

	delete _codec;
	_codec = nullptr;
}

void DIBDecoder::loadPalette(Common::SeekableReadStream &stream) {
	uint16 steps = stream.size() / 6;
	_palette.resize(steps, false);

	for (uint8 i = 0; i < steps; i++) {
		byte r = stream.readByte();
		stream.readByte();

		byte g = stream.readByte();
		stream.readByte();

		byte b = stream.readByte();
		stream.readByte();
		_palette.set(i, r, g, b);
	}
}

bool DIBDecoder::loadStream(Common::SeekableReadStream &stream) {
	//stream.hexdump(stream.size());
	uint32 headerSize = stream.readUint32LE();
	if (headerSize != 40)
		return false;

	int32 width = stream.readSint32LE();
	int32 height = stream.readSint32LE();
	if (height < 0) {
		warning("BUILDBOT: height < 0 for DIB");
	}
	stream.readUint16LE(); // planes
	_bitsPerPixel = stream.readUint16LE();
	uint32 compression = stream.readUint32BE();
	/* uint32 imageSize = */ stream.readUint32LE();
	/* int32 pixelsPerMeterX = */ stream.readSint32LE();
	/* int32 pixelsPerMeterY = */ stream.readSint32LE();
	uint32 paletteColorCount = stream.readUint32LE();
	/* uint32 colorsImportant = */ stream.readUint32LE();

	paletteColorCount = (paletteColorCount == 0) ? 255: paletteColorCount;
	_palette.resize(paletteColorCount, false);

	Common::SeekableSubReadStream subStream(&stream, 40, stream.size());

	_codec = Image::createBitmapCodec(compression, 0, width, height, _bitsPerPixel);

	if (!_codec)
		return false;

	_surface = _codec->decodeFrame(subStream);

	// The DIB decoder converts 1bpp images to the 16-color equivalent; we need them to be the palette extrema
	// in order to work.
	if (_bitsPerPixel == 1) {
		for (int y = 0; y < _surface->h; y++) {
			for (int x = 0; x < _surface->w; x++) {
				*const_cast<byte *>((const byte *)_surface->getBasePtr(x, y)) = *(const byte *)_surface->getBasePtr(x, y) == 0xf ? 0x00 : 0xff;
			}
		}
	}

	// For some reason, DIB cast members have the palette indices reversed
	if (_bitsPerPixel == 8) {
		for (int y = 0; y < _surface->h; y++) {
			for (int x = 0; x < _surface->w; x++) {
				// We're not su[pposed to modify the image that is coming from the decoder
				// However, in this case, we know what we're doing.
				*const_cast<byte *>((const byte *)_surface->getBasePtr(x, y)) = 255 - *(const byte *)_surface->getBasePtr(x, y);
			}
		}
	}

	return true;
}

/****************************
* BITD
****************************/

BITDDecoder::BITDDecoder(int w, int h, uint16 bitsPerPixel, uint16 pitch, const byte *palette, uint16 version) : _palette(0) {
	_surface = new Graphics::Surface();
	_pitch = pitch;
	_version = version;

	int minPitch = ((w * bitsPerPixel) >> 3) + ((w * bitsPerPixel % 8) ? 1 : 0);
	if (_pitch < minPitch) {
		warning("BITDDecoder: pitch is too small (%d < %d), graphics will decode wrong", _pitch, minPitch);

		_pitch = minPitch;
	}
	byte Bpp = bitsPerPixel >> 3;
	Graphics::PixelFormat format;
	switch (Bpp) {
	case 0:
	case 1:
		// 8-bit palette
		format = Graphics::PixelFormat::createFormatCLUT8();
		break;
	case 2:
		// RGB555
		format = Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0);
		break;
	case 4:
		// RGB888
		format = Graphics::PixelFormat(4, 8, 8, 8, 0, 16, 8, 0, 0);
		break;
	default:
		warning("BITDDecoder::BITDDecoder(): unsupported bpp %d", bitsPerPixel);
		break;
	}

	_surface->create(w, h, format);

	// TODO: Bring this in from the main surface?
	_palette.resize(255, false);
	_palette.set(palette, 0, 255);

	_bitsPerPixel = bitsPerPixel;
}

BITDDecoder::~BITDDecoder() {
	destroy();
}

void BITDDecoder::destroy() {
	_surface->free();
	delete _surface;
	_surface = nullptr;
}

void BITDDecoder::loadPalette(Common::SeekableReadStream &stream) {
	// no op
}

bool BITDDecoder::loadStream(Common::SeekableReadStream &stream) {
	int x = 0, y = 0;

	Common::Array<byte> pixels;
	// Unpacking bodges for D3 and below
	bool skipCompression = false;
	uint32 bytesNeed = _pitch * _surface->h;
	if (_bitsPerPixel != 1) {
		if (_version < kFileVer300) {
			bytesNeed = _surface->w * _surface->h * _bitsPerPixel / 8;
			skipCompression = stream.size() >= bytesNeed;
		} else if (_version < kFileVer400) {
			bytesNeed = _surface->w * _surface->h * _bitsPerPixel / 8;
			// for D3, looks like it will round up the _surface->w to align 2
			// not sure whether D2 will have the same logic.
			// check lzone-mac data/r-c/tank.a-1 and lzone-mac data/r-a/station-b.01.
			if (_surface->w & 1)
				bytesNeed += _surface->h * _bitsPerPixel / 8;
			skipCompression = stream.size() == bytesNeed;
		}
	}
	skipCompression |= (stream.size() == bytesNeed);

	// If the stream has exactly the required number of bits for this image,
	// we assume it is uncompressed.
	if (skipCompression) {
		debugC(6, kDebugImages, "Skipping compression");
		for (int i = 0; i < stream.size(); i++) {
			pixels.push_back((int)stream.readByte());
		}
	} else {
		while (!stream.eos()) {
			// TODO: D3 32-bit bitmap casts seem to just be ARGB pixels in a row and not RLE.
			// Determine how to distinguish these different types. Maybe stage version.
			// for D4, 32-bit bitmap is RLE, and the encoding format is every line contains the a? r g b at the same line of the original image.
			// i.e. for every line, we shall combine 4 parts to create the original image.
			if (_bitsPerPixel == 32 && _version < kFileVer400) {
				int data = stream.readByte();
				pixels.push_back(data);
			} else {
				int data = stream.readByte();
				int len = data + 1;
				if ((data & 0x80) != 0) {
					len = ((data ^ 0xFF) & 0xff) + 2;
					data = stream.readByte();
					for (int p = 0; p < len; p++) {
						pixels.push_back(data);
					}
				} else {
					for (int p = 0; p < len; p++) {
						data = stream.readByte();
						pixels.push_back(data);
					}
				}
			}
		}
	}

	if (pixels.size() < bytesNeed) {
		uint32 tail = bytesNeed - pixels.size();

		warning("BITDDecoder::loadStream(): premature end of stream (srcSize: %d, targetSize: %d, expected: %d, w: %d, h: %d, pitch: %d, bitsPerPixel: %d)",
				(int)stream.size(), pixels.size(), pixels.size() + tail, _surface->w, _surface->h, _pitch, _bitsPerPixel);

		for (uint32 i = 0; i < tail; i++)
			pixels.push_back(0);
	}

	int offset = 0;
	if (_bitsPerPixel == 8 && _surface->w < (int)(pixels.size() / _surface->h))
		offset = (pixels.size() / _surface->h) - _surface->w;
	// looks like the data want to round up to 2, so we either got offset 1 or 0.
	// but we may met situation when the pixel size is exactly equals to w * h, thus we add a check here.
	if (offset)
		offset = _surface->w % 2;

	debugC(5, kDebugImages, "BITDDecoder::loadStream: unpacked %d bytes, width: %d, height: %d, pitch: %d, bitsPerPixel: %d", pixels.size(), _surface->w, _surface->h, _pitch, _bitsPerPixel);
	if (debugChannelSet(8, kDebugImages)) {
		Common::hexdump(pixels.data(), (int)pixels.size());
	}

	uint32 color;

	if (pixels.size() > 0) {
		for (y = 0; y < _surface->h; y++) {
			for (x = 0; x < _surface->w;) {
				switch (_bitsPerPixel) {
				case 1:
					for (int c = 0; c < 8 && x < _surface->w; c++, x++) {
						color = (pixels[(y * _pitch) + (x >> 3)] & (1 << (7 - c))) ? 0xff : 0x00;
						*((byte *)_surface->getBasePtr(x, y)) = color;
					}
					break;
				case 2:
					for (int c = 0; c < 4 && x < _surface->w; c++, x++) {
						color = (pixels[(y * _pitch) + (x >> 2)] & (0x3 << (2 * (3 - c)))) >> (2 * (3 - c));
						*((byte *)_surface->getBasePtr(x, y)) = color;
					}
					break;
				case 4:
					for (int c = 0; c < 2 && x < _surface->w; c++, x++) {
						color = (pixels[(y * _pitch) + (x >> 1)] & (0xf << (4 * (1 - c)))) >> (4 * (1 - c));
						*((byte *)_surface->getBasePtr(x, y)) = color;
					}
					break;
				case 8:
					*((byte *)_surface->getBasePtr(x, y)) = pixels[(y * _surface->w) + x + (y * offset)];
					x++;
					break;

				case 16:
					if (skipCompression) {
						color = (pixels[((y * _surface->w) * 2) + x * 2]) << 8 |
							(pixels[((y * _surface->w) * 2) + x * 2 + 1]);
					} else {
						color =	(pixels[((y * _surface->w) * 2) + x]) << 8 |
							(pixels[((y * _surface->w) * 2) + (_surface->w) + x]);
					}
					*((uint16 *)_surface->getBasePtr(x, y)) = color;
					x++;
					break;

				case 32:
					// if we have the issue in D3 32bpp images, then the way to fix it should be the same as 16bpp images.
					// check the code above, there is different behaviour between in D4 and D3. Currently we are only using D4.
					if (skipCompression) {
						color = pixels[(((y * _surface->w * 4)) + (x * 4 + 1))] << 16 |
							pixels[(((y * _surface->w * 4)) + (x * 4 + 2))] << 8 |
							pixels[(((y * _surface->w * 4)) + (x * 4 + 3))];
					} else {
						color = pixels[(((y * _surface->w * 4)) + (x + _surface->w))] << 16 |
							pixels[(((y * _surface->w * 4)) + (x + 2 * _surface->w))] << 8 |
							pixels[(((y * _surface->w * 4)) + (x + 3 * _surface->w))];
					}
					*((uint32 *)_surface->getBasePtr(x, y)) = color;
					x++;
					break;

				default:
					x++;
					break;
				}
			}
		}
	}

	return true;
}

void copyStretchImg(const Graphics::Surface *srcSurface, Graphics::Surface *targetSurface, const Common::Rect &srcRect, const Common::Rect &targetRect, const byte *pal) {
	if (!(srcSurface) || !(targetSurface))
		return;
	if ((srcSurface->h <= 0) || (srcSurface->w <= 0)) {
		// Source area is nonexistant
		return;
	}

	Graphics::Surface *temp1 = nullptr;
	Graphics::Surface *temp2 = nullptr;
	// Convert source surface to target colourspace (if required)
	if (srcSurface->format.bytesPerPixel != g_director->_wm->_pixelformat.bytesPerPixel) {
		temp1 = srcSurface->convertTo(g_director->_wm->_pixelformat, g_director->_wm->getPalette(), g_director->_wm->getPaletteSize(), g_director->_wm->getPalette(), g_director->_wm->getPaletteSize());
	}
	// Nearest-neighbour scale source surface to target dimensions (if required)
	if (targetRect.width() != srcRect.width() || targetRect.height() != srcRect.height()) {
		temp2 = (temp1 ? temp1 : srcSurface)->scale(targetRect.width(), targetRect.height(), false);
	}
	targetSurface->copyFrom(*(temp2 ? temp2 : (temp1 ? temp1 : srcSurface)));
	if (temp1) {
		temp1->free();
		delete temp1;
	}
	if (temp2) {
		temp2->free();
		delete temp2;
	}
}


} // End of namespace Director
