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
 * Drawn for the 320x240 Pimoroni Display Pack 2.0.
 */

#include <math.h>
#include <stdio.h>

#include "cassette_anim.hpp"

using namespace pimoroni;

// Reel geometry
static constexpr int reel_y = 104;
static constexpr int reel_lx = 122;
static constexpr int reel_rx = 198;
static constexpr int hub_r = 12;
static constexpr int hub_hole_r = 9;
// Radius of the tape pack on an empty / full reel
static constexpr int pack_min_r = 14;
static constexpr int pack_max_r = 34;
// Angular speed (radians per frame) of a reel with pack radius 1, the actual
// speed is inversely proportional to the pack radius so that the tape moves
// with a constant linear speed, like in the real thing.
static constexpr float spin_k = 6.0f;

static constexpr float two_pi = 6.2831853f;
static constexpr float sixth = two_pi / 6.0f;

void CassetteAnim::init(PicoGraphics &g) {
	shell       = g.create_pen(0x2A, 0x2B, 0x30);
	shell_light = g.create_pen(0x55, 0x58, 0x60);
	screw       = g.create_pen(0xB8, 0xBC, 0xC4);
	label_bg    = g.create_pen(0xF2, 0xEA, 0xD2);
	stripe1     = g.create_pen(0xE0, 0x5A, 0x1A);
	stripe2     = g.create_pen(0x1E, 0x8C, 0x9A);
	ink         = g.create_pen(0x20, 0x20, 0x28);
	window      = g.create_pen(0x14, 0x16, 0x1C);
	tape        = g.create_pen(0x6A, 0x3A, 0x22);
	hub         = g.create_pen(0xEC, 0xEC, 0xEC);
}

void CassetteAnim::rounded_rect(PicoGraphics &g, const Rect &r, int radius) {
	g.rectangle(Rect(r.x + radius, r.y, r.w - 2*radius, r.h));
	g.rectangle(Rect(r.x, r.y + radius, r.w, r.h - 2*radius));
	g.circle(Point(r.x + radius, r.y + radius), radius);
	g.circle(Point(r.x + r.w - radius - 1, r.y + radius), radius);
	g.circle(Point(r.x + radius, r.y + r.h - radius - 1), radius);
	g.circle(Point(r.x + r.w - radius - 1, r.y + r.h - radius - 1), radius);
}

void CassetteAnim::draw_reel(PicoGraphics &g, int cx, int cy, int r, float angle) {
	// Tape pack
	g.set_pen(tape);
	g.circle(Point(cx, cy), r);
	// Hub
	g.set_pen(hub);
	g.circle(Point(cx, cy), hub_r);
	g.set_pen(window);
	g.circle(Point(cx, cy), hub_hole_r);
	// Six teeth pointing inwards, these show the rotation
	g.set_pen(hub);
	for(int i = 0; i < 6; i++) {
		float a = angle + i*sixth;
		Point tip(cx + (int)lroundf(4.0f*cosf(a)), cy + (int)lroundf(4.0f*sinf(a)));
		Point b1(cx + (int)lroundf(hub_hole_r*cosf(a - 0.3f)), cy + (int)lroundf(hub_hole_r*sinf(a - 0.3f)));
		Point b2(cx + (int)lroundf(hub_hole_r*cosf(a + 0.3f)), cy + (int)lroundf(hub_hole_r*sinf(a + 0.3f)));
		g.triangle(tip, b1, b2);
	}
}

static void draw_screw(PicoGraphics &g, Pen head, Pen slot, int x, int y) {
	g.set_pen(head);
	g.circle(Point(x, y), 4);
	g.set_pen(slot);
	g.line(Point(x - 2, y + 2), Point(x + 3, y - 3));
}

void CassetteAnim::draw(PicoGraphics &g, Pen bg, std::string_view label, uint32_t progress, bool spinning) {
	if(progress > progress_max)
		progress = progress_max;

	g.set_pen(bg);
	g.clear();

	// Shell with a lighter edge
	g.set_pen(shell_light);
	rounded_rect(g, Rect(8, 12, 304, 216), 12);
	g.set_pen(shell);
	rounded_rect(g, Rect(10, 14, 300, 212), 11);

	// Bottom part of the shell with the head opening, holes and the tape
	g.set_pen(shell_light);
	g.triangle(Point(64, 226), Point(84, 168), Point(236, 168));
	g.triangle(Point(64, 226), Point(236, 168), Point(256, 226));
	g.set_pen(window);
	g.circle(Point(108, 206), 6);
	g.circle(Point(212, 206), 6);
	g.rectangle(Rect(146, 186, 28, 12));
	g.set_pen(tape);
	g.rectangle(Rect(92, 176, 136, 3));
	g.set_pen(hub);
	g.circle(Point(92, 177), 5);
	g.circle(Point(228, 177), 5);

	// Label
	g.set_pen(label_bg);
	rounded_rect(g, Rect(24, 24, 272, 124), 6);
	g.set_pen(stripe1);
	g.rectangle(Rect(24, 50, 272, 6));
	g.set_pen(stripe2);
	g.rectangle(Rect(24, 56, 272, 4));

	// Label texts: file name, progress and the side letter
	// The name comes padded with spaces ("NAME    .CAS"), drop them
	char name[16];
	size_t nl = 0;
	for(char c : label)
		if(c != ' ' && nl < sizeof(name))
			name[nl++] = c;
	g.set_pen(ink);
	g.text(std::string_view(name, nl), Point(32, 30), 320, 2, 0.0f, 0, true);
	char pct[8];
	int n = snprintf(pct, sizeof(pct), "%u%%", (unsigned int)(progress/10));
	g.text(std::string_view(pct, n), Point(288 - n*16, 30), 320, 2, 0.0f, 0, true);
	g.text("A", Point(36, 92), 320, 3, 0.0f, 0, true);

	// Play / pause indicator
	if(spinning) {
		g.set_pen(stripe2);
		g.triangle(Point(262, 90), Point(262, 116), Point(284, 103));
	} else {
		g.set_pen(stripe1);
		g.rectangle(Rect(262, 90, 8, 26));
		g.rectangle(Rect(276, 90, 8, 26));
	}

	// Window through which the tape packs are visible
	g.set_pen(ink);
	g.rectangle(Rect(72, 66, 176, 76));
	g.set_pen(window);
	g.rectangle(Rect(74, 68, 172, 72));

	// Tape pack sizes, the tape area is preserved when it moves
	// from the left reel to the right one
	constexpr float area = (float)(pack_max_r*pack_max_r - pack_min_r*pack_min_r);
	float p = (float)progress / progress_max;
	float rl = sqrtf(pack_min_r*pack_min_r + area*(1.0f - p));
	float rr = sqrtf(pack_min_r*pack_min_r + area*p);

	if(spinning) {
		// Clockwise rotation (the screen y axis points down)
		angle_l += spin_k / rl;
		angle_r += spin_k / rr;
		if(angle_l >= two_pi) angle_l -= two_pi;
		if(angle_r >= two_pi) angle_r -= two_pi;
	}

	draw_reel(g, reel_lx, reel_y, (int)lroundf(rl), angle_l);
	draw_reel(g, reel_rx, reel_y, (int)lroundf(rr), angle_r);

	// Screws
	draw_screw(g, screw, shell, 18, 22);
	draw_screw(g, screw, shell, 301, 22);
	draw_screw(g, screw, shell, 18, 217);
	draw_screw(g, screw, shell, 301, 217);
	draw_screw(g, screw, shell, 160, 214);
}
