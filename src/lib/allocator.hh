/*
 * $File: allocator.hh
 * $Date: Sat Apr 12 20:03:08 2014 +0000
 * $Author: Xinyu Zhou <zxytim[at]gmail[dot]com>
 */

#pragma once

/*
 * Scheduling when memory size is limited.
 *
 */
#include <emmintrin.h>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <limits>
#include <condition_variable>
#include <map>
#include <string>
#include <fstream>
#include <memory>
#include <set>
#include <iostream>

#include "debugutils.h"

#define assert_equal(a, b) \
	do { \
		auto __a = (a); \
		auto __b = (b); \
		if (__a != __b) { \
			std::cerr << "assert equal failed." << std::endl; \
			std::cerr << __FILE__ << ":" << __func__ << ":" << __LINE__ << std::endl; \
			std::cerr << #a << ":" << __a << std::endl; \
			std::cerr << #b << ":" << __b << std::endl; \
		} \
	} while (0)

#define assert_not_equal(a, b) \
	do { \
		auto __a = (a); \
		auto __b = (b); \
		if (__a == __b) { \
			std::cerr << "assert not equal failed." << std::endl; \
			std::cerr << __FILE__ << ":" << __func__ << ":" << __LINE__ << std::endl; \
			std::cerr << #a << ":" << __a << std::endl; \
			std::cerr << #b << ":" << __b << std::endl; \
		} \
	} while (0)

class SIGMODAllocator {
	public:
		static const size_t free_mem_size_max = 10000;

		void enter(std::mutex &lock, size_t id) {
			lock.lock();

			std::thread th(std::bind(&SIGMODAllocator::wait_mem, this, std::ref(lock), id, 0, true));
			th.detach();

			lock.lock();
			lock.unlock();
		}

		void exit() {
			{
				std::lock_guard<std::mutex> guard(nr_free_slots_mutex);
				nr_free_slots ++;
			}
			std::thread th(std::bind(&SIGMODAllocator::schedule, this));
			th.detach();
		}

		void *alloc(std::mutex &lock, size_t id, size_t size) {
			while (true) {
				// XXX: debug

				size_t mem_total = get_mem_total();
				size_t fake_mem_total = 15lu * 1024 * 1024 * 1024;
				size_t used = mem_total - get_free_mem();
				if (fake_mem_total >= used + reserve_mem) {
					size_t fake_free_mem = fake_mem_total - used;
					if (size <= fake_free_mem) {
						fprintf(stderr, "size: %lu free_mem_size_max: %lu\n", size, fake_free_mem);
						char *data = (char *)_mm_malloc(size, 16);
						if (data) { // succeed
							memset(data, 0, size);
							return data;
						}
					}
				}

				// failed
				lock.lock();
				std::thread th(std::bind(&SIGMODAllocator::wait_mem, this, std::ref(lock), id, size, false));
				th.detach();
				lock.lock();
				lock.unlock();
			}
		};

		void dealloc(void *data) {
			_mm_free(data);
		}

		static void init(int nr_threads, size_t reserve_mem) {
			assert(!allocator); // XXX
			allocator = new SIGMODAllocator(nr_threads, reserve_mem);
		}

		static SIGMODAllocator *get_instance() {
			return allocator;
		};

	protected:
		static SIGMODAllocator *allocator;

		SIGMODAllocator(int nr_threads, size_t reserve_mem) {
			this->nr_threads = nr_threads;
			nr_free_slots = nr_threads;
			this->reserve_mem = reserve_mem;
		}


		struct Acquisition {
			Acquisition(std::mutex &lock, size_t id, size_t mem_size, bool is_enter) :
				lock(lock), id(id), mem_size(mem_size), is_enter(is_enter) {
			}
			std::mutex &lock;
			size_t id, mem_size;
			bool is_enter;
		};

		struct AcquisitionBiggerSizeFirst {
			bool operator () (const Acquisition &a, const Acquisition &b) const {  // bigger size first
				if (a.mem_size != b.mem_size)
					return a.mem_size > b.mem_size;
				return a.id < b.id;
			}
		};

		struct AcquisitionCmpByID {
			bool operator () (const Acquisition &a, const Acquisition &b) const {  // bigger size first
				return a.id < b.id;
			}
		};

		size_t nr_free_slots;
		size_t nr_threads;
		size_t reserve_mem;

		std::set<Acquisition, AcquisitionBiggerSizeFirst> acquisitions_bigger_size_first;
		std::mutex acquisitions_bigger_size_first_mutex,
			free_mem_mutex, nr_free_slots_mutex;

		void wait_mem(std::mutex &lock, size_t id, size_t size, bool is_enter) {
			{
				std::lock_guard<std::mutex> guard(acquisitions_bigger_size_first_mutex);
				acquisitions_bigger_size_first.emplace(lock, id, size, is_enter);
			}
			{
//                if (!is_enter) {
//                    std::lock_guard<std::mutex> guard(nr_free_slots_mutex);
//                    nr_free_slots ++;
//                }
			}
			schedule();
		}


		void schedule() {
			std::lock_guard<std::mutex> lock(acquisitions_bigger_size_first_mutex);

//            size_t free_mem = get_free_mem();
			size_t free_mem = free_mem_size_max; // XXX: debug
			for (auto it = acquisitions_bigger_size_first.begin(); it != acquisitions_bigger_size_first.end(); ) {
				{
					std::lock_guard<std::mutex> nr_free_slots_lock(nr_free_slots_mutex);
					if (nr_free_slots == 0)
						break;
				}
				auto it_next = it;
				it_next ++;
				{
					bool unlock = false;
					if (it->is_enter || (!it->is_enter && free_mem >= it->mem_size + reserve_mem)) {
						unlock = true;
					}
					if (unlock) {
						fprintf(stderr, "unlock: free_mem: %lu reserve_mem: %lu required_mem: %lu\n",
								free_mem, reserve_mem, it->mem_size);
						if (!it->is_enter)
							free_mem -= it->mem_size;

						fprintf(stderr, "start running %lu ...\n", it->id);
						it->lock.unlock();  // the thread starts runing
						{
							std::lock_guard<std::mutex> nr_free_slots_lock(nr_free_slots_mutex);
							nr_free_slots --;
						}
						acquisitions_bigger_size_first.erase(it);
					}
				}
				it = it_next;
			}
			fprintf(stderr, "free_mem: %lu\n", free_mem);
			fprintf(stderr, "nr_free_slots: %lu\n", nr_free_slots);
			fprintf(stderr, "waiting acquisitions: %lu\n", acquisitions_bigger_size_first.size());
			if (!acquisitions_bigger_size_first.empty() && nr_free_slots == nr_threads) {
				fprintf(stderr, "ERROR: unable to schedule\n");
			}
		}

		static size_t get_free_mem() {
			// return in byte;
			std::ifstream fin("/proc/meminfo");
			std::string str;

			size_t num_max = std::numeric_limits<size_t>::max();
			size_t nfree = num_max, ncache = num_max;
			while (not fin.eof()) {
				fin >> str;
				if (str == "MemFree:") {
					fin >> str;
					nfree = stoi(str);

					std::cout << "MemFree: " << str << std::endl;

				} else if (str == "Cached:") {
					fin >> str;
					ncache = stoi(str);

					std::cout << "Cached: " << str << std::endl;
				}
			}
			fin.close();
			if (nfree == num_max || ncache == num_max)
				return 0;
			return (nfree + ncache) * 1024;
		}

		static size_t get_mem_total() {
			// return in byte;
			std::ifstream fin("/proc/meminfo");
			std::string str;

			while (not fin.eof()) {
				fin >> str;
				if (str == "MemTotal:") {
					fin >> str;
					return stoi(str) * 1024lu;
				}
			}
			return 0;
		}

};

class Allocator {
	public:
		Allocator() : id(get_unique_id()) {
		}
		size_t get_id() const { return this->id; }
		void *alloc(size_t mem_size) {
			return SIGMODAllocator::get_instance()->alloc(lock, id, mem_size);
		}
		void dealloc(void *data) {
			SIGMODAllocator::get_instance()->dealloc(data);
		}

		void enter() {
			SIGMODAllocator::get_instance()->enter(lock, id);
		}
		void exit() {
			SIGMODAllocator::get_instance()->exit();
		}

	protected:
		size_t id;
		std::mutex lock;

		static size_t get_unique_id() {
			static size_t cnt = 0;
			static std::mutex cnt_mutex;
			size_t ret;
			{
				std::lock_guard<std::mutex> lock(cnt_mutex);
				ret = cnt ++;
			}
			return ret;
		}
};



/**
 * vim: syntax=cpp11 foldmethod=marker
 */
