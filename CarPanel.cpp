// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <chrono>
#include <cassert>
#include <cinttypes>
#include <exception>

#include "CarPanel.hpp"

#include <stdio.h>

Timer::Timer(int64_t time) {
	_handle = CreateWaitableTimer(nullptr, false, nullptr);
	if (!_handle)
		throw std::system_error(GetLastError(), std::system_category());

	// in 100 nanoseconds intervals
	LARGE_INTEGER due_time = { .QuadPart = time * -100 };
	if (!SetWaitableTimer(_handle, &due_time, time, nullptr, nullptr, false))
		throw std::system_error(GetLastError(), std::system_category());
}

Timer::~Timer() {
	CloseHandle(_handle);
}

bool Timer::check() {
	DWORD result = WaitForSingleObject(_handle, 0);
	switch (result) {
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT:
			return false;
		case WAIT_ABANDONED:
		case WAIT_FAILED:
		default:
			throw std::system_error(GetLastError(), std::system_category());
	}
}

FrameBuffer::FrameBuffer(uint32_t address)
{
	_tx_address.sin_family = AF_INET;
	_tx_address.sin_addr.s_addr = address;
	_tx_address.sin_port = Network::htons()(666);

	if (address == INADDR_BROADCAST)
		_socket.set_broadcast(true);

	std::memset(&_request, 0, sizeof(_request));
	_request.count = 4;
	_request.frame[0].control.CU = 0;	// Low current drain mode
	_request.frame[0].control.P = 0;	// Number of General Purpose Outputs
	_request.frame[0].control.DR = 1;	// bias 1/3 vs 1/2
	_request.frame[0].control.SC = 0;	// display off
	_request.frame[0].control.BU = 0;	// Pover saving mode
	_request.frame[0].control.DD = 0;
	_request.frame[1].control.DD = 2;
	_request.frame[2].control.DD = 1;
	_request.frame[3].control.DD = 3;
}

const uint8_t big_start[8] = { 13, 29, 45, 61, 77, 93, 109, 125 }; // 143 Jedynka
const uint8_t small_start[3] = { 188, 172, 156 }; // 143 Jedynka
							//    a    b    c    d    e    f    g    h    i  J  k  l    m  n
const uint8_t big_segs[14] =	{ 7,   0,   2,   0xA, 0xE, 0xC, 0xD, 1,   8, 3, 4, 6,   5, 9 };
const uint8_t small_segs[14] =	{ 4,   0xC, 0xE, 7,   2,   0,   1,   0xD, 5, 3, 8, 0xA, 9, 6 };

const uint16_t LCDChars[128] = { // 127 znaków, Zaprojektowane dla wyœwietlacza 14 segmentowego, przerobione znaki na 13 segmentowy
		0x003F, 0x0006, 0x005B, 0x004F, 0x0066, 0x006D, 0x007D, 0x0007, 0x007F, 0x006F,
		0x0130, 0x2038, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0509, 0x0202, 0x12C6, 0x12ED, 0x2424, 0x0D5D, 0x0200,
		0x0C00, 0x2100, 0x3F40, 0x12C0, 0x2000, 0x00C0, 0x0800, 0x2400, 0x243F, 0x0406,
		0x00DB, 0x00CF, 0x00E6, 0x00ED, 0x00FD, 0x0007, 0x00FF, 0x00EF, 0x1200, 0x2200,
		0x0C00, 0x00C8, 0x2100, 0x1421, 0x105F, 0x00F7, 0x128F, 0x0039, 0x120F, 0x00F9,
		0x00F1, 0x00BD, 0x00F6, 0x1209, 0x001E, 0x0C70, 0x0038, 0x0536, 0x0936, 0x003F,
		0x00F3, 0x083F, 0x08F3, 0x018D, 0x1201, 0x003E, 0x2430, 0x2836, 0x2D00, 0x2500,
		0x2409, 0x0039, 0x0900, 0x000F, 0x2800, 0x0008, 0x0100, 0x1058, 0x0878, 0x00D8,
		0x208E, 0x2058, 0x14C0, 0x018F, 0x1070, 0x1000, 0x000E, 0x1E00, 0x1208, 0x10D4,
		0x1050, 0x00DC, 0x0170, 0x0486, 0x0050, 0x0888, 0x0078, 0x001C, 0x2010, 0x101C,
		0x2D00, 0x028E, 0x2048, 0x2149, 0x1200, 0x0C89, 0x00C0
};

// Send data to the display
void FrameBuffer::send() {
	int len = _request.count * sizeof(Frame) + 1;

	_socket.sendto(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&_request),
				   len), 0, &_tx_address, sizeof(_tx_address));
}

void FrameBuffer::set(int bit, bool a, bool x) {
	int bank = bit / 52;
	bit %= 52;

	int index = bit / 8;
	bit %= 8;
	uint8_t mask = (1 << bit);
	if (!a)
		_request.frame[bank].data[index] &= ~mask;
	
	if (x)
		_request.frame[bank].data[index] ^= mask;
}


Animator::Animator(int delay)
	: _delay(delay), _delay_counter(0)
{
}

IAnimator::Status Animator::animate(FrameBuffer* buffer) {
	if (_delay_counter++ < _delay)
		return NO_CHANGE;
	_delay_counter = 0;

	return do_animate(buffer);
}

void Animator::clear(FrameBuffer* buffer, const uint8_t* segs, int count) {
	for (int i = 0; i < count; i++)
		buffer->set(*segs++, false, false);
}


Text::Text(const uint8_t* start_map, int width, const uint8_t* seg_map)
	: _width(width), _start_map(start_map), _seg_map(seg_map)
{
}

void Text::write(FrameBuffer* buffer, const char* text) {
	int i;
	for (i = _width - 1; (i >= 0) && *text; i--, text++)
		write_char(buffer, i, *text);

	for (; i >= 0; i--)
		write_char(buffer, i, ' ');
}

void Text::write_char(FrameBuffer* buffer, int pos, char c) {
	int start = _start_map[pos];
	uint16_t mask = LCDChars[c];

	for (int i = 0; i < 14; i++, mask >>= 1) {
		if (i == 9) continue;
		buffer->set(start + _seg_map[i], false, mask & 1);
	}
}


TextShift::TextShift(const uint8_t* start_map, int width, const uint8_t* seg_map, int delay, int shift)
	: Text(start_map, width, seg_map), Animator(delay), _pos(0), _shift(shift)
{
}

void TextShift::set_text(std::string text) {
	_text = text;
	reset();
}

IAnimator::Status TextShift::do_animate(FrameBuffer* buffer) {
	if ((_pos + _width) > _text.length())
		_pos = 0;

	write(buffer, _text.c_str() + _pos);
	Status ret = !_pos ? UPDATE_SYNC : UPDATE;

	_pos += _shift;

	return ret;
}

void TextShift::init(FrameBuffer* buffer) {
	_pos = 0;
	do_animate(buffer);
}

// 64. Kreseczki od D - BASS
const uint8_t bass_segs[] = { 96, 92, 88, 72 };

Bar::Bar(const uint8_t* segs, int seg_count, int delay)
	: Animator(delay), _pos(0), _dir(0), _segs(segs), _seg_count(seg_count)
{ }

void Bar::clear(FrameBuffer* buffer) {
	Animator::clear(buffer, _segs, _seg_count);
	_pos = 0;
}

IAnimator::Status Bar::do_animate(FrameBuffer* buffer) {
	buffer->set(_segs[_pos], true, true);
	_pos += _dir ? -1 : 1;

	if ((_pos >= _seg_count) || (_pos < 0)) {
		_dir ^= 1;
		_pos += _dir ? -1 : 1;
	}

	return (_dir || _pos) ? UPDATE : UPDATE_SYNC;
}

const uint8_t circle_segs[] = {
	144,	// kó³ko góra
	145,	// kó³ko prawo góra
	146,	// kó³ko prawo
	147,	// kó³ko prawo dó³
	151,	// kó³ko dó³
	155,	// kó³ko lewo dó³
	152,	// kó³ko lewo
	148,	// kó³ko lewo góra
};

Circle::Circle(int delay)
	: Animator(delay), _pos(0), cnt(0)
{ }

void Circle::clear(FrameBuffer* buffer) {
	Animator::clear(buffer, circle_segs, std::size(circle_segs));
	_pos = 0;
}

IAnimator::Status Circle::do_animate(FrameBuffer* buffer) {
	buffer->set(circle_segs[_pos++], true, true);
	if (_pos >= 8) _pos = 0;
	return _pos ? UPDATE : UPDATE_SYNC;
}

const uint8_t carousel_segs[] = {
	203,	// Dolby ?
	199,	// ATA
	191,	// kaseta
	187,	// coœ kwadratowe
	183,	// coœ okr¹g³e
	136,	// DISC
	128,	// TRACK
	120,	// ST
	112,	// AF
	108,	// TA
	104,	// TP
	16,		// BANK
	12,		// REP
	24,		// (REP)1
	28,		// (REP)2
	32,		// SHUF
	40,		// (SHUF)1
	48,		// (SHUF)2
	56,		// (SHUF)ALL
	140,	// Czerwona ramka wokó³ ko³ka
//};

//const uint8_t border_segs[] = {
	141,	// górne i lewe obramowanie kó³ka
	142,	// prawe i dolne obramowanie kó³ka
//};

//const uint8_t arrow_segs[] = {
	149,	// stra³ka w górê
	153,	// strza³ka w lewo
	154,	// Strza³ka w dó³
	150,	// Strza³ka w prawo
};

Carousel::Carousel(const uint8_t* segs, unsigned int seg_count, int delay, unsigned int lag)
	: Animator(delay), _pos(1+lag), _segs(segs), _seg_count(seg_count), _lag(lag)
{}

void Carousel::clear(FrameBuffer* buffer) {
	Animator::clear(buffer, _segs, _seg_count);
	_pos = 1 + _lag;
}

void Carousel::init(FrameBuffer* buffer) {
	for (unsigned int i = 0; i <= _lag; i++)
		buffer->set(_segs[i], false, true);
}

IAnimator::Status Carousel::do_animate(FrameBuffer* buffer) {
	int off = _pos - _lag - 1;
	if (off >= 0)
		buffer->set(_segs[off], true, true);
	else
		buffer->set(_segs[_seg_count + off], true, true);

	buffer->set(_segs[_pos++], true, true);

	if (_pos >= _seg_count) {
		_pos = 0;
	}

	return _pos ? UPDATE : UPDATE_SYNC;
}


void Time::update() {
	//structure to store system time (in usual time format)
	FILETIME ltime;
	//structure to store local time (local time in 64 bits)
	FILETIME ftTimeStamp;

	GetSystemTimeAsFileTime(&ftTimeStamp); //Gets the current system time
	FileTimeToLocalFileTime(&ftTimeStamp, &ltime);//convert in local time and store in ltime
	FileTimeToSystemTime(&ltime, &stime);//convert in system time and store in stime
}

BigTime::BigTime()
	: Text(big_start, 8, big_segs), Animator(10)
{}

/*
76. dolna kropka od dwu kropka
80. górna kropka od dwu kropka
44 prawy przecinek
*/

IAnimator::Status BigTime::do_animate(FrameBuffer* buffer) {
	char str[9];
	Status ret;
	Time::update();
	bool update = _sec != stime.wSecond;
	ret = update ? UPDATE_SYNC : UPDATE;
	_sec = stime.wSecond;
	bool colon = stime.wMilliseconds >= 500;
	update |= _msec != colon;
	_msec = colon;

	if (!update)
		return NO_CHANGE;
#if 0
	// Data jest ca³kowicie nie czytelna
	buffer->set(76, false, true);
	buffer->set(44, false, true);
	snprintf(str, sizeof(str), "%4d%02d%02d", stime.wYear, stime.wMonth, stime.wDay);
#else
	buffer->set(80, false, colon);
	buffer->set(76, false, colon);
	snprintf(str, sizeof(str), "%02d %02d %02d", stime.wHour, stime.wMinute, stime.wSecond);

#endif
	write(buffer, str);

	return ret;
}


SmallTime::SmallTime()
	: Text(small_start, 3, small_segs), Animator(10)
{}


/*
159. Godzina segment b
167. Godzina segment c
171. dwójka od godziny
175. dwukropek od godziny
*/
IAnimator::Status SmallTime::do_animate(FrameBuffer* buffer) {
	char str[9];
	Status ret;
	Time::update();
	bool update = _sec != stime.wSecond;
	ret = update ? UPDATE_SYNC : UPDATE;
	_sec = stime.wSecond;
	bool colon = stime.wMilliseconds >= 500;
	update |= _msec != colon;
	_msec = colon;

	if (!update)
		return NO_CHANGE;

	buffer->set(175, false, colon);
	buffer->set(159, false, stime.wHour >= 10);
	buffer->set(171, false, stime.wHour >= 20);
	buffer->set(167, false, (stime.wHour >= 10) && (stime.wHour < 20));
	snprintf(str, sizeof(str), "%02d%02d", stime.wHour, stime.wMinute);
	write(buffer, str+1);
	return ret;
}

void SmallTime::clear(FrameBuffer* buffer) {
	uint8_t segs[] = { 175, 159, 171, 167 };

	Animator::clear(buffer, segs, std::size(segs));
}


void AnimatorList::add(IAnimator* animator) {
	_list.push_back(animator);
}

IAnimator::Status AnimatorList::animate(FrameBuffer* buffer) {
	Status update = NO_CHANGE;

	for (auto a : _list)
		if (a->animate(buffer) != NO_CHANGE)
			update = UPDATE;

	return update;
}

void AnimatorList::init(FrameBuffer* buffer) {
	for (auto a : _list)
		a->init(buffer);
}

AnimatorCarousel::AnimatorCarousel()
	: _elapsed(0)
{}

void AnimatorCarousel::add(IAnimator* animator, int duration) {
	_list.emplace_back(AnimEntry{ animator, duration });
	_item = _list.begin();
}

IAnimator::Status AnimatorCarousel::animate(FrameBuffer* buffer) {

	Status update = _item->animator->animate(buffer);

	if (update == UPDATE_SYNC) {
		_elapsed++;
		if (_elapsed >= _item->duration) { // Init zliczamy jako pierwsz¹ synchronizacjê
			_elapsed = 0;
			_item->animator->clear(buffer);
			if (++_item == _list.end()) {
				_item = _list.begin();
			}
			_item->animator->init(buffer);
			return UPDATE_SYNC;
		}
		return UPDATE;
	}

	return update;
}

CarPanel::CarPanel(uint32_t address)
	: _buffer(address)
{ }

#include <conio.h>

void CarPanel::animate() {
#if 1
	AnimatorCarousel main_text;
#if 1
	TextShift main1(big_start, 8, big_segs, 10);
	main1.set_text("A POZNIEJ JEST Z GORKI WPISUJESZ JUZ CALE NAPISY");
	//main1.set_text("        CZESC        W TELEWIZJI NUDA? POPACZ NA MNIE        CHCIALEM CI ZYCZYC MILEGO DNIA        KOCHAM CIE MUFFINALKO - Dziub       ");
	TextShift main2(big_start, 8, big_segs, 100, 8);
	main2.set_text("SIEMANKOMILO CIEWIDZIEC          DZIEN    DOBRY          MILEGO   DNIA           KOCHAM    CIE  SKARBIE         ");
#else
	TextShift main1(big_start, 8, big_segs, 10);
	main1.set_text("        CZESC         MILO CIE WIDZIEC        ");
	TextShift main2(big_start, 8, big_segs, 100, 8);
	main2.set_text(" CZESC  MILO CIEWIDZIEC          DZIEN    DOBRY          MILEGO   DNIA          ");
#endif
	TextShift main3(big_start, 8, big_segs, 200, 8);
	main3.set_text(" PIATEK 3 MARCA ");

	AnimatorList list;
	BigTime main_time;
	main_text.add(&main_time, 20);
	main_text.add(&main1, 2);
	main_text.add(&main_time, 5);
	main_text.add(&main2, 1);
	main_text.add(&main_time, 5);
	main_text.add(&main3, 1);
	list.add(&main_text);

	TextShift small_shift(small_start, 3, small_segs, 15);
	small_shift.set_text("   BYPCI SIE I MUFFI SIE  ");
	SmallTime small_time;
	AnimatorCarousel small_text;
	small_text.add(&small_time, 10);
	small_text.add(&small_shift, 5);
	list.add(&small_text);

	Bar b(bass_segs, std::size(bass_segs), 5);
	list.add(&b);
	Carousel cc(carousel_segs, std::size(carousel_segs), 70);
	list.add(&cc);


	Circle circle1(10);
	Carousel circle2(circle_segs, std::size(circle_segs), 10, 5);
	Carousel circle3(circle_segs, std::size(circle_segs), 3, 1);
	Bar circle4(circle_segs, std::size(circle_segs), 10);
	AnimatorCarousel circle;
	circle.add(&circle1, 7);
	circle.add(&circle2, 7);
	circle.add(&circle3, 15);
	circle.add(&circle4, 3);
	list.add(&circle);
	/*
	Carousel border(border_segs, std::size(border_segs), 10);
	list.add(&border);
	Carousel arrows(arrow_segs, std::size(arrow_segs), 20);
	list.add(&arrows);
	*/


	// Kreseczki od D BASS
	_buffer.set(64, false, true);

	list.init(&_buffer);
	while (1) {
		if (list.animate(&_buffer))
			_buffer.send();

		Sleep(10);
	}
#else
	while (1) {
		for (int i = 0; i < 204; i++) {
			_buffer.set(i, true, true);
			_buffer.send();
			//Sleep(10);
			printf("bit %d\n", i);
			_getch();
		}
	}
#endif
}
