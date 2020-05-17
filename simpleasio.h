#pragma once

#include "asio.hpp"
#include <map>

const int max_buf_size = 65535;


class CSimpleAsio
{
public:
	CSimpleAsio() = delete;
	CSimpleAsio(std::string ip, std::string port, bool isServer);
	~CSimpleAsio();
public:
	void Run();
	void Send(const char* buf, const int& size, const std::string& msg);
	std::string GetState() const;
private:
	void _Accept();
	void _ServerSend(const char* buf, const int& size, const std::string& msg);
	void _ServerReceive(std::shared_ptr<asio::ip::tcp::socket> client);

	void _Connect();
	void _ClientSend(const char* buf, const int& size, const std::string& msg);
	void _ClientReceive();
public:
	asio::io_context m_io_context_;
	std::thread m_thread;
	std::atomic<bool> m_appEnable;
	bool m_isServer;
	std::string m_ip;
	std::string m_port;
	asio::steady_timer m_state_timer;
	asio::io_context::strand m_strand;
	std::map<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<std::array<char, max_buf_size>>> m_client_bufs;


	std::shared_ptr<asio::io_service::work> m_work;
	// 服务端模式
	std::shared_ptr<asio::ip::tcp::acceptor> m_acceptor_;
	std::map<std::shared_ptr<asio::ip::tcp::socket>, std::string> m_client_id_map;

	// 客户端模式
	std::shared_ptr<asio::ip::tcp::socket> m_client_;

};