/*
 * SingleSlotQueue.h
 *
 *      Author: Gregory Kramida
 *     License: Apache v2
 *   Copyright: (c) Gregory Kramida 2015
 */

#ifndef RECO_UTILS_SINGLESLOTQUEUE_H_
#define RECO_UTILS_SINGLESLOTQUEUE_H_
#pragma once

//standard
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>

namespace reco {
namespace utils {

/**
 * Abstract 1-slot queue
 **/
template<typename T> class swap_buffer {

public:
	virtual void push_back(const T& item) = 0;
	virtual T pop_front() = 0;
	protected:
	swap_buffer() {
	}
	;
	virtual ~swap_buffer() {
	}
	;

};

/**
 * A thread-safe 1-slot queue with pessimistic locking
 **/
template<typename T> class pessimistic_swap_buffer:
		public swap_buffer<T> {
public:
	pessimistic_swap_buffer();
	virtual ~pessimistic_swap_buffer();
	virtual void push_back(const T& item);
	virtual T pop_front();

private:
	std::mutex mutex;
	std::condition_variable cv_push;
	std::condition_variable cv_pop;
	bool empty;

	T storage[1];
};

template<typename T> pessimistic_swap_buffer<T>::pessimistic_swap_buffer() :
		mutex(),
				cv_push(),
				cv_pop(),
				empty(true) {
}

template<typename T> pessimistic_swap_buffer<T>::~pessimistic_swap_buffer() {

}

template<typename T> void pessimistic_swap_buffer<T>::push_back(const T& item) {
	std::unique_lock<std::mutex> lock(mutex);
	while (!empty) {
		cv_push.wait(lock);
	}
	memcpy(&storage[0], &item, sizeof(T));
	empty = false;
	cv_pop.notify_one();
}

template<typename T> T pessimistic_swap_buffer<T>::pop_front() {
	std::unique_lock<std::mutex> lock(mutex);
	while (empty) {
		cv_pop.wait(lock);
	}
	T ret;
	memcpy(&ret, &storage[0], sizeof(T));
	empty = true;
	cv_push.notify_one();
	return ret;
}

/**
 * A thread-safe 1-slot queue with optimistic waiting (requires non-0 items)
 **/
template<typename T> class optimistic_swap_queue:
		public swap_buffer<T> {
public:
	optimistic_swap_queue();
	virtual ~optimistic_swap_queue();
	virtual void push_back(const T& item);
	virtual T pop_front();

private:
	std::atomic_bool allow_pop;
	std::atomic_bool allow_push;
	std::atomic<T> storage_a;
	T storage[1];
};

template<typename T> optimistic_swap_queue<T>::optimistic_swap_queue() :
		allow_pop(false),
				allow_push(true) {
}

template<typename T> optimistic_swap_queue<T>::~optimistic_swap_queue() {

}

template<typename T> void optimistic_swap_queue<T>::push_back(const T& item) {
	bool exp;
	//wait for item to get popped
	do {
		exp = true;
	} while (!allow_push.compare_exchange_weak(exp, false)); //cork up the bottle as soon as we get through
	memcpy(&storage[0], &item, sizeof(T));
	allow_pop.store(true);
}

template<typename T> T optimistic_swap_queue<T>::pop_front() {
	bool exp;
	//wait for item to get pushed
	do {
		exp = true;
	} while (!allow_pop.compare_exchange_weak(exp, false)); //cork up the bottle as soon as we get through
	T ret;
	memcpy(&ret, &storage[0], sizeof(T));
	allow_push.store(true);
	return ret;
}

} // end namespace utils
} // end namespace reco

#endif /* MODULES_VSTAR_SINGLESLOTQUEUE_H_ */
