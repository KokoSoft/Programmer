/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __CARPANEL_HPP__
#define __CARPANEL_HPP__

#include <cstdint>
#include <random>
#include <array>
#include <chrono>
#include <queue>

#include "Network.hpp"
#include "TargetTester.hpp"

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

class FrameBuffer {
	public:
		FrameBuffer(uint32_t address);

		void set(int bit, bool a, bool x);
		// Send data to the display
		void send();

	private:
		SocketUDP _socket;
		struct sockaddr_in _tx_address;

		__pragma(pack(push, 1));
		typedef struct {
			uint16_t : 4; // last 4 bits of LCD data
			uint16_t : 2; // reserved = 0
			uint16_t CU : 1;	// Low current drain mode
			uint16_t P : 4;		// Number of General Purpose Outputs
			uint16_t DR : 1;	// bias 1/3 vs 1/2
			uint16_t SC : 1;	// display off
			uint16_t BU : 1;	// Pover saving mode
			uint16_t DD : 2; // Warning! Reversed endian
		} Control;

		typedef struct {
			union {
				uint8_t data[7];
				struct {
					uint16_t reserved[3];
					Control control;
				};
			};
		} Frame;

		typedef struct {
			uint8_t count;
			Frame frame[4];
		} Request;
		__pragma(pack(pop));

		static_assert(sizeof(Frame) == 8);
		static_assert(sizeof(Request) == (8*4+1));

		Request _request;
};

class IAnimator {
	public:
		typedef enum { NO_CHANGE, UPDATE, UPDATE_SYNC } Status;

		virtual Status animate(FrameBuffer* buffer) = 0;
		virtual void init(FrameBuffer* buffer) {};
		virtual void clear(FrameBuffer* buffer) {};
};

class Animator : public IAnimator {
	public:
		Animator(int delay);
		virtual Status animate(FrameBuffer* buffer) override;
		virtual void init(FrameBuffer* buffer) override {
			do_animate(buffer);
		}
		void reset() {
			_delay_counter = 0;
		}

	protected:
		virtual Status do_animate(FrameBuffer* buffer) = 0;
		static void clear(FrameBuffer* buffer, const uint8_t* segs, int count);

		int _delay_counter, _delay;
};

class Text {
	public:
		Text(const uint8_t* start_map, int width, const uint8_t* seg_map);

		void write(FrameBuffer* buffer, const char* text);

	private:
		void write_char(FrameBuffer* buffer, int pos, char c);

		const uint8_t* const _start_map;
		const uint8_t* const _seg_map;

	protected:
		int _width;
};

class TextShift : public Text, public Animator {
	public:
		TextShift(const uint8_t* start_map, int width, const uint8_t* seg_map, int delay, int shift = 1);

		void set_text(std::string text);
		virtual void init(FrameBuffer* buffer) override;

		void reset() {
			_pos = 0;
			Animator::reset();
		}

	private:
		virtual Status do_animate(FrameBuffer* buffer);

		std::string _text;
		int _pos, _shift;
};

class Circle : public Animator {
	public:
		Circle(int delay);
		virtual void clear(FrameBuffer* buffer) override;

	private:
		virtual Status do_animate(FrameBuffer* buffer);
		unsigned int _pos, cnt;
};

class Bar : public Animator {
	public:
		Bar(const uint8_t* segs, int seg_count, int delay);
		virtual void clear(FrameBuffer* buffer) override;

	private:
		virtual Status do_animate(FrameBuffer* buffer);
		unsigned int _pos, _dir;
		const uint8_t* const _segs;
		const unsigned int _seg_count;
};

class Carousel : public Animator {
	public:
		Carousel(const uint8_t* segs, unsigned int seg_count, int delay, unsigned int lag = 0);
		virtual void init(FrameBuffer* buffer) override;
		virtual void clear(FrameBuffer* buffer) override;

	private:
		virtual Status do_animate(FrameBuffer* buffer);
		unsigned int _pos;
		const uint8_t* const _segs;
		const unsigned int _seg_count, _lag;
};

class Time {
	public:
		SYSTEMTIME stime;
	protected:
		WORD _sec;
		bool _msec;
		void update();
};

class BigTime : protected Text, protected Time, public Animator {
	public:
		BigTime();
	
	private:
		virtual Status do_animate(FrameBuffer* buffer);
};

class SmallTime : protected Text, protected Time, public Animator {
	public:
		SmallTime();
		virtual void clear(FrameBuffer* buffer) override;

	private:
		virtual Status do_animate(FrameBuffer* buffer);
};

class AnimatorList : public IAnimator {
	public:
		void add(IAnimator* animator);
		virtual Status animate(FrameBuffer* buffer) override;
		virtual void init(FrameBuffer* buffer) override;

	protected:
		std::vector<IAnimator*> _list;
};

class AnimatorCarousel : public IAnimator {
	public:
		AnimatorCarousel();

		void add(IAnimator* animator, int duration);
		virtual Status animate(FrameBuffer* buffer) override;

	protected:
		typedef struct {
			IAnimator* animator;
			int duration;
		} AnimEntry;

		typedef std::vector<AnimEntry> AnimVector;
		AnimVector _list;
		AnimVector::iterator _item;

		int _elapsed;
};

class CarPanel {
	public:
		CarPanel(uint32_t address);

		void animate();
	private:
		FrameBuffer _buffer;
};

#endif /* __CARPANEL_HPP__ */
