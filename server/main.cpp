#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

// Level 2 OrderBook: price -> quantity
std::map<double, int, std::greater<double>> buy_book;  // Descending for buy
std::map<double, int> sell_book;                      // Ascending for sell

struct Order {
	std::string side; // "buy" or "sell"
	double price;
	int quantity;
};

constexpr int NUM_THREADS = 4;

// Single-writer matching engine: orders are submitted to this queue
std::deque<Order> order_queue;
std::mutex order_queue_mutex;
std::condition_variable order_queue_cv;
std::atomic<bool> shutdown_engine{false};
std::atomic<long long> processed_count{0};

// Simple matching: match marketable orders, aggregate at price levels
void process_order(const Order& order) {
	if (order.side == "buy") {
		// Try to match with lowest sell
		while (!sell_book.empty() && order.price >= sell_book.begin()->first && order.quantity > 0) {
			auto it = sell_book.begin();
			int match_qty = std::min(order.quantity, it->second);
			it->second -= match_qty;
			if (it->second == 0) sell_book.erase(it);
			// For latency, we don't print fills here
		}
		if (order.quantity > 0) buy_book[order.price] += order.quantity;
	} else if (order.side == "sell") {
		// Try to match with highest buy
		while (!buy_book.empty() && order.price <= buy_book.begin()->first && order.quantity > 0) {
			auto it = buy_book.begin();
			int match_qty = std::min(order.quantity, it->second);
			it->second -= match_qty;
			if (it->second == 0) buy_book.erase(it);
			// For latency, we don't print fills here
		}
		if (order.quantity > 0) sell_book[order.price] += order.quantity;
	}
}

// Submit an order to the matching engine queue
void submit_order(const Order& order) {
	{
		std::lock_guard<std::mutex> lock(order_queue_mutex);
		order_queue.push_back(order);
	}
	order_queue_cv.notify_one();
}

// Matching engine: single thread owns the book and processes queued orders
void matching_engine() {
	while (true) {
		Order next;
		{
			std::unique_lock<std::mutex> lock(order_queue_mutex);
			order_queue_cv.wait(lock, [] { return !order_queue.empty() || shutdown_engine.load(); });
			if (shutdown_engine.load() && order_queue.empty()) {
				break;
			}
			next = order_queue.front();
			order_queue.pop_front();
		}

		process_order(next);
		processed_count.fetch_add(1, std::memory_order_relaxed);
	}
}

void print_top_levels() {
	std::cout << "Top of Book:\n";
	if (!buy_book.empty())
		std::cout << "Best Bid: " << buy_book.begin()->first << " x " << buy_book.begin()->second << std::endl;
	if (!sell_book.empty())
		std::cout << "Best Ask: " << sell_book.begin()->first << " x " << sell_book.begin()->second << std::endl;
}

void self_test() {
	constexpr int NUM_ORDERS = 100000;
	std::mt19937 rng(42);
	std::uniform_real_distribution<double> price_dist(99.0, 101.0);
	std::uniform_int_distribution<int> qty_dist(1, 100);
	std::uniform_int_distribution<int> side_dist(0, 1);

	std::vector<Order> orders;
	orders.reserve(NUM_ORDERS);
	for (int i = 0; i < NUM_ORDERS; ++i) {
		Order order;
		order.side = side_dist(rng) ? "buy" : "sell";
		order.price = price_dist(rng);
		order.quantity = qty_dist(rng);
		orders.push_back(order);
	}

	processed_count.store(0, std::memory_order_relaxed);
	auto start = std::chrono::high_resolution_clock::now();

	auto worker = [&](int tid) {
		int chunk = NUM_ORDERS / NUM_THREADS;
		int begin = tid * chunk;
		int end = (tid == NUM_THREADS - 1) ? NUM_ORDERS : begin + chunk;
		for (int i = begin; i < end; ++i) {
			submit_order(orders[i]);
		}
	};

	std::vector<std::thread> threads;
	for (int t = 0; t < NUM_THREADS; ++t) {
		threads.emplace_back(worker, t);
	}
	for (auto& th : threads) th.join();

	// Wait until the matching engine processes all orders
	while (processed_count.load(std::memory_order_relaxed) < NUM_ORDERS) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	auto end = std::chrono::high_resolution_clock::now();
	double total_sec = std::chrono::duration<double>(end - start).count();
	double throughput = NUM_ORDERS / total_sec;

	std::cout << "Processed " << NUM_ORDERS << " orders in " << total_sec << " seconds\n";
	std::cout << "Throughput: " << throughput << " orders/sec\n";
	print_top_levels();
}

int main() {
	// Start matching engine thread
	std::thread engine_thread(matching_engine);

	std::cout << "Running self-test...\n";
	self_test();
	std::cout << "\nServer listening on 127.0.0.1:54000..." << std::endl;
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		std::cerr << "WSAStartup failed: " << iResult << std::endl;
		shutdown_engine.store(true);
		order_queue_cv.notify_all();
		engine_thread.join();
		return 1;
	}

	SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
		WSACleanup();
		shutdown_engine.store(true);
		order_queue_cv.notify_all();
		engine_thread.join();
		return 1;
	}

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = htons(54000);

	if (bind(ListenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		std::cerr << "bind() failed." << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		shutdown_engine.store(true);
		order_queue_cv.notify_all();
		engine_thread.join();
		return 1;
	}

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Error listening on socket." << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		shutdown_engine.store(true);
		order_queue_cv.notify_all();
		engine_thread.join();
		return 1;
	}

	SOCKET ClientSocket;
	sockaddr_in clientInfo;
	int clientInfoSize = sizeof(clientInfo);

	auto client_handler = [&](SOCKET s, sockaddr_in addr) {
		char recvbuf[1024];
		while (true) {
			int bytesReceived = recv(s, recvbuf, (int)sizeof(recvbuf) - 1, 0);
			if (bytesReceived <= 0) break;
			recvbuf[bytesReceived] = '\0';
			std::istringstream iss{std::string(recvbuf)};
			std::string side; double price; int quantity;
			if (iss >> side >> price >> quantity) {
				submit_order({side, price, quantity});
				std::string ack = "Order received\n";
				send(s, ack.c_str(), (int)ack.size(), 0);
			} else {
				std::string nack = "Invalid order format\n";
				send(s, nack.c_str(), (int)nack.size(), 0);
			}
		}
		closesocket(s);
		std::cout << "Client disconnected " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl << std::flush;
	};

	while ((ClientSocket = accept(ListenSocket, (SOCKADDR*)&clientInfo, &clientInfoSize)) != INVALID_SOCKET) {
		std::cout << "Client connected from " << inet_ntoa(clientInfo.sin_addr) << ":" << ntohs(clientInfo.sin_port) << std::endl << std::flush;
		std::thread(client_handler, ClientSocket, clientInfo).detach();
	}

	closesocket(ListenSocket);
	WSACleanup();

	// Shutdown matching engine cleanly
	shutdown_engine.store(true);
	order_queue_cv.notify_all();
	engine_thread.join();
	return 0;
} 