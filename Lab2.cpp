#include <iostream>
#include <chrono>
#include <random>
#include <thread> 
#include "thread_pool.h"

std::random_device rd;  // a seed source for the random number engine
std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
std::uniform_int_distribution<> distrib(5000, 10000);

/*
1. ��� ������ ������������� 4-�� �������� �������� � �� ���� �����
���������. ������ ��������� ������ � ����� ����� ���������. �������
������� ��������� ���������� ��������� ������ (�� ���� � ������� �
��� �������� ������ ���������� �� ID, �� ����� ID ����� ��������
��������� ��������� �� ������ ������). ������ �������� �� ��������� �
������ ������ �� �������� ������� �������� ������. ������ �����
���������� ��� �� 5 �� 10 ������.
*/

size_t task() {
    size_t time = distrib(gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(time));
    return time;
}

int main()
{
    int task_count = 10;
    thread_pool pool;
    pool.initialize(4, true);
    //std::cout << "Starting " << task_count << " tasks." << std::endl;
    for (int i = 0; i < task_count; i++) {
        size_t id = pool.add_task(task);
        //std::cout << "Added task " << id << " to the thread pool." << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(8));
    pool.terminate_now();
    /*
    while(true) {
        size_t id;
        std::cout << std::endl << "Enter task ID to check status: ";
        std::cin >> id;
        if (id == -1) {
            break;
        }
        size_t status = pool.get_status(id);
        if (status != 0) {
            std::cout << "Result: " << status << std::endl;
        }
    }
    */
}