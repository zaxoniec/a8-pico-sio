/*
 * This file is part of the a8-pico-sio project --
 * An Atari 8-bit SIO drive and (turbo) tape emulator for
 * Raspberry Pi Pico, see
 *
 *         https://github.com/woj76/a8-pico-sio
 *
 * For information on what / whose work it is based on, check the corresponding
 * source files and the README file. This file is licensed under GNU General
 * Public License 3.0 or later.
 *
 * Full screen, color, animated cassette shown while the C: device is playing.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include "libraries/pico_graphics/pico_graphics.hpp"

class CassetteAnim {
public:
	// Progress is given in 1/progress_max units
	static constexpr uint32_t progress_max = 1000;

	// Allocates the extra palette entries, call once after the main
	// UI pens have been created (the P4 palette has 16 entries in total,
	// this takes 10 of them).
	void init(pimoroni::PicoGraphics &g);

	// Draws one full screen frame. The current font of g is used for the label.
	// bg - background pen (the UI theme color),
	// label - text written on the cassette label (the file name),
	// progress - how much of the tape has been played (0..progress_max),
	// spinning - true when the motor is on, the reels then rotate and each
	// call advances the animation by one frame.
	void draw(pimoroni::PicoGraphics &g, pimoroni::Pen bg, std::string_view label,
		uint32_t progress, bool spinning);

private:
	pimoroni::Pen shell, shell_light, screw, label_bg, stripe1, stripe2, ink, window, tape, hub;
	float angle_l = 0.0f, angle_r = 0.0f;

	void rounded_rect(pimoroni::PicoGraphics &g, const pimoroni::Rect &r, int radius);
	void draw_reel(pimoroni::PicoGraphics &g, int cx, int cy, int r, float angle);
};
