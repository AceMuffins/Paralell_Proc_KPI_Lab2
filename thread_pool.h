#pragma once
#include "task_queue.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

class thread_pool
{
public:
	inline thread_pool() = default;
	inline ~thread_pool() { terminate(); }
public:
	void initialize(const size_t worker_count, bool debug_mode);
	void terminate();
	void terminate_now();
	void debug_terminate();
	void routine();
	bool working() const;
	bool working_unsafe() const;
public:
	template <typename task_t, typename... arguments>
	inline size_t add_task(task_t&& task, arguments&&... parameters);
	size_t get_status(size_t id);
public:
	thread_pool(const thread_pool& other) = delete;
	thread_pool(thread_pool&& other) = delete;
	thread_pool& operator=(const thread_pool& rhs) = delete;
	thread_pool& operator=(thread_pool&& rhs) = delete;
private:
	struct TaskStatus {
		enum Status {
			Waiting,
			Working,
			Finished
		} status;
		size_t result = NULL;
	};
	mutable read_write_lock m_rw_lock;
	mutable read_write_lock print_lock;
	mutable std::condition_variable_any m_task_waiter;
	std::vector<std::thread> m_workers;
	task_queue<std::function<size_t()>> m_tasks;
	std::unordered_map<size_t, TaskStatus> task_id_status;
	std::unordered_map<size_t, std::chrono::time_point<std::chrono::system_clock>> debug_queue_time;
	double queue_wait_time = 0.0;
	size_t tasks_processed = 0;
	bool m_initialized = false;
	bool m_terminated = false;
	bool DEBUG = false;
};

bool thread_pool::working() const
{
	read_lock _(m_rw_lock);
	return working_unsafe();
}
bool thread_pool::working_unsafe() const
{
	return m_initialized && !m_terminated;
}

void thread_pool::initialize(const size_t worker_count, bool debug_mode = false)
{
	write_lock _(m_rw_lock);
	if (m_initialized || m_terminated)
	{
		return;
	}
	DEBUG = debug_mode;
	if (DEBUG == true) {
		print_lock.lock();
		printf("STR: Initializing %zu workers.\n", worker_count);
		print_lock.unlock();
	}
	m_workers.reserve(worker_count);
	for (size_t id = 0; id < worker_count; id++)
	{
		m_workers.emplace_back(&thread_pool::routine, this);
	}
	m_initialized = !m_workers.empty();
}

void thread_pool::routine()
{
	while (true)
	{
		bool task_acquired = false;
		size_t task_id = -1;
		std::function<size_t()> task;
		{
			write_lock _(m_rw_lock);
			auto wait_condition = [this, &task_acquired, &task_id, &task] {
				task_acquired = m_tasks.pop(task, task_id);
				return m_terminated || task_acquired;
				};
			m_task_waiter.wait(_, wait_condition);
		}
		if (m_terminated && !task_acquired)
		{
			return;
		}
		task_id_status[task_id].status = thread_pool::TaskStatus::Status::Working;
		if (DEBUG == true) {
			print_lock.lock();
			auto time_now = std::chrono::system_clock::now();
			auto elapsed = duration_cast<nanoseconds>(time_now - debug_queue_time[task_id]);
			queue_wait_time += elapsed.count() * 1e-6;
			printf("WRK: Task ID %2zu began working. Queue wait time %.3f miliseconds.\n", task_id, elapsed.count() * 1e-6);
			print_lock.unlock();
		}
		task_id_status[task_id].result = task();
		task_id_status[task_id].status = thread_pool::TaskStatus::Status::Finished;
		if (DEBUG == true) {
			print_lock.lock();
			tasks_processed++;
			printf("END: Task ID %2zu returned %zu.\n", task_id, task_id_status[task_id].result);
			print_lock.unlock();
		}
	}
}
template <typename task_t, typename... arguments>
size_t thread_pool::add_task(task_t&& task, arguments&&... parameters)
{
	{
		read_lock _(m_rw_lock);
		if (!working_unsafe()) {
			return -1;
		}
	}
	auto bind = std::bind(std::forward<task_t>(task),
		std::forward<arguments>(parameters)...);
	size_t id = m_tasks.emplace(bind);
	task_id_status[id].status = thread_pool::TaskStatus::Status::Waiting;
	m_task_waiter.notify_one();
	if (DEBUG == true) {
		print_lock.lock();
		printf("ADD: Task ID %2zu was added to the queue.\n", id);
		debug_queue_time[id] = std::chrono::system_clock::now();
		print_lock.unlock();
	}
	return id;
}


size_t thread_pool::get_status(size_t id)
{
	if (task_id_status.count(id) == 0) {
		std::cout << "No such task exists." << std::endl;
		return 0;
	}
	auto task_status = task_id_status.at(id);
	if (task_status.status == thread_pool::TaskStatus::Status::Waiting) {
		std::cout << "Task " << id << " is in the task queue." << std::endl;
	}
	else if (task_status.status == thread_pool::TaskStatus::Status::Working) {
		std::cout << "Task " << id << " is being processed." << std::endl;
		return 0;
	}
	return task_status.result;

}

void thread_pool::terminate()
{
	if (DEBUG == true) {
		print_lock.lock();
		printf("TRM: Terminate called.\n");
		print_lock.unlock();
	}
	{
		write_lock _(m_rw_lock);
		if (working_unsafe())
		{
			if (DEBUG == true) {
				print_lock.lock();
				printf("TRM: Waiting for tasks to finish.\n");
				print_lock.unlock();
			}
			m_terminated = true;
		}
		else
		{
			if (DEBUG == true) {
				debug_terminate();
			}
			m_workers.clear();
			m_terminated = false;
			m_initialized = false;
			return;
		}
	}
	m_task_waiter.notify_all();
	for (std::thread& worker : m_workers)
	{
		worker.join();
	}
	if (DEBUG == true) {
		debug_terminate();
	}
	m_workers.clear();
	m_terminated = false;
	m_initialized = false;
}

inline void thread_pool::terminate_now()
{
	if (DEBUG == true) {
		print_lock.lock();
		printf("TRM: Urgent termination called.\n");
		printf("TRM: Clearing the task queue.\n");
		print_lock.unlock();
	}
	{
		write_lock _(m_rw_lock);
		m_tasks.clear();
		if (working_unsafe())
		{
			if (DEBUG == true) {
				print_lock.lock();
				printf("TRM: Waiting for tasks to finish.\n");
				print_lock.unlock();
			}
			m_terminated = true;
		}
		else
		{
			m_workers.clear();
			m_terminated = false;
			m_initialized = false;
			return;
		}
	}
	m_task_waiter.notify_all();
	for (std::thread& worker : m_workers)
	{
		worker.join();
	}
	m_workers.clear();
	m_terminated = false;
	m_initialized = false;
}

void thread_pool::debug_terminate() {
	print_lock.lock();

	printf("TRM: No tasks left, terminating.\n\n");
	printf("====DEBUG INFO====\n");
	printf("Tasks added: %zu\n", m_tasks.task_count());
	printf("Tasks processed: %zu\n", tasks_processed);
	printf("Total queue wait time: %.3f ms\n", queue_wait_time);
	printf("Average queue wait time: %.3f ms\n", queue_wait_time / tasks_processed);

	print_lock.unlock();
}