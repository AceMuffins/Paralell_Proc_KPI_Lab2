#pragma once

#include <queue>
#include <thread>
#include <shared_mutex>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

template <typename task_type_t>
class task_queue
{
	using task_queue_implementation = std::queue<task_type_t>;
public:
	inline task_queue() = default;
	inline ~task_queue() { clear(); }
	inline bool empty() const;
	inline size_t size() const;
	inline size_t task_count();
public:
	inline void clear();
	inline bool pop(task_type_t& task, size_t& id);
	template <typename... arguments>
	inline size_t emplace(arguments&&... parameters);
public:
	task_queue(const task_queue& other) = delete;
	task_queue(task_queue&& other) = delete;
	task_queue& operator=(const task_queue& rhs) = delete;
	task_queue& operator=(task_queue&& rhs) = delete;
private:
	mutable read_write_lock m_rw_lock;
	task_queue_implementation m_tasks;
	std::queue<size_t> m_ids;
	size_t tasks_total = 0;
};

template <typename task_type_t>
bool task_queue<task_type_t>::empty() const
{
	read_lock _(m_rw_lock);
	return m_tasks.empty();
}

template <typename task_type_t>
size_t task_queue<task_type_t>::size() const
{
	read_lock _(m_rw_lock);
	return m_tasks.size();
}

template<typename task_type_t>
inline size_t task_queue<task_type_t>::task_count()
{
	return tasks_total;
}

template <typename task_type_t>
void task_queue<task_type_t>::clear()
{
	write_lock _(m_rw_lock);
	while (!m_tasks.empty())
	{
		m_tasks.pop();
		m_ids.pop();
	}
}

template <typename task_type_t>
bool task_queue<task_type_t>::pop(task_type_t& task, size_t& id)
{
	write_lock _(m_rw_lock);
	if (m_tasks.empty())
	{
		return false;
	}
	else
	{
		task = std::move(m_tasks.front());
		id = std::move(m_ids.front());
		m_tasks.pop();
		m_ids.pop();
		return true;
	}
}

template <typename task_type_t>
template <typename... arguments>
size_t task_queue<task_type_t>::emplace(arguments&&... parameters)
{
	write_lock _(m_rw_lock);
	size_t id = tasks_total;
	m_tasks.emplace(std::forward<arguments>(parameters)...);
	m_ids.push(id);
	tasks_total++;
	return id;
}
