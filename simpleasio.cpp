#include "simpleasio.h"


CSimpleAsio::CSimpleAsio(std::string ip, std::string port, bool isServer)
	: m_ip(ip)
	, m_port(port)
	, m_appEnable(true)
	, m_isServer(isServer)
	, m_state_timer(m_io_context_)
	, m_strand(m_io_context_)
{

}

CSimpleAsio::~CSimpleAsio()
{
	if (m_thread.joinable())
	{
		if (WAIT_TIMEOUT == WaitForSingleObject(m_thread.native_handle(), 1))
		{
			m_io_context_.post([this]()
				{
					m_appEnable = false;
					if (m_isServer)
					{
						m_acceptor_->close();
						for (auto it : m_client_id_map)
						{
							if (it.first->is_open())
							{
								it.first->close();
							}
						}
						
					}
					else
					{
						if (m_client_->is_open())
						{
							m_client_->close();
						}				
					}
					m_work.reset();
				});
		}
		m_thread.join();
	}
}

void CSimpleAsio::Run()
{
	m_thread = std::thread([this]()
		{
			if (m_isServer)
			{
				m_acceptor_ = std::make_shared<asio::ip::tcp::acceptor>(m_io_context_, asio::ip::tcp::endpoint(asio::ip::address::from_string(m_ip.c_str()), atoi(m_port.c_str())));
				_Accept();
			}
			else
			{
				m_client_ = std::make_shared<asio::ip::tcp::socket>(m_io_context_);
				_Connect();
			}



			m_work = std::make_shared<asio::io_service::work>(asio::io_service::work(m_io_context_));
			try
			{
				m_io_context_.run();
			}
			catch (std::exception& e)
			{

			};
		});
}

void CSimpleAsio::Send(const char* buf, const int& size, const std::string& msg)
{
	if (!m_appEnable)
	{
		return;
	}

	if (m_isServer)
	{
		_ServerSend(buf, size, msg);
	}
	else
	{
		_ClientSend(buf, size, msg);
	}
}

std::string CSimpleAsio::GetState() const
{
	std::string ret;
	if (m_isServer)
	{
		ret.append("Mode: Server. ");
		ret.append("Remote clients: ");
		for (auto it : m_client_id_map)
		{
			ret.append(it.second);
			ret.append(" | ");
		}
	}
	else
	{
		ret.append("Mode: Client. ");
		ret.append("Remote Server: ");

		if (m_client_ != nullptr)
		{
			if (m_client_->is_open())
			{
				ret.append(m_ip);
				ret.append(": ");
				ret.append(m_port);
			}
		}
	}

	return ret;
}

void CSimpleAsio::_Accept()
{
	if (!m_appEnable)
	{
		return;
	}

	for (auto it = m_client_id_map.begin(); it != m_client_id_map.end();)
	{
		if (!(*it).first->is_open())
		{
			m_client_id_map.erase(it++);

		}
		else
		{
			it++;
		}
	}

	std::shared_ptr<asio::ip::tcp::socket> client = std::make_shared<asio::ip::tcp::socket>(m_io_context_);
	if (client == nullptr || m_acceptor_ == nullptr)
	{
		return;
	}
	m_acceptor_->async_accept(*client, m_strand.wrap([this, client](std::error_code ec) {
		if (!ec)
		{
			auto id = client->remote_endpoint().address().to_string() + ": " + std::to_string(client->remote_endpoint().port());


			m_client_id_map[client] = id;

			_ServerReceive(client);
		}



		if (m_appEnable)
		{
			_Accept();
		}
		}));
}

void CSimpleAsio::_ServerSend(const char* buf, const int& size, const std::string& msg)
{
	if (!m_appEnable)
	{
		return;
	}

	if (buf == nullptr || m_client_id_map.empty())
	{

		return;
	}

	for (auto it = m_client_id_map.begin(); it != m_client_id_map.end();)
	{
		if (!(*it).first->is_open())
		{
			m_client_id_map.erase(it++);

		}
		else
		{
			it++;
		}
	}

	for (auto& it : m_client_id_map)
	{
		if (it.first->is_open())
		{
			it.first->async_send(asio::buffer(buf, size), m_strand.wrap([this, it, msg](asio::error_code ec, std::size_t len) {
				if (ec)
				{

					it.first->close();

				}
				else
				{

				}
				}));
		}
	}
}

void CSimpleAsio::_ServerReceive(std::shared_ptr<asio::ip::tcp::socket> client)
{
	if (client == nullptr)
	{
		return;
	}

	if (!m_appEnable)
	{
		return;
	}

	if (!client->is_open())
	{

		return;
	}

	if (m_client_bufs.find(client) == m_client_bufs.end())
	{
		std::shared_ptr<std::array<char, max_buf_size>> buf = std::make_shared<std::array<char, max_buf_size>>();
		m_client_bufs[client] = buf;
	}
	ZeroMemory(m_client_bufs[client]->data(), m_client_bufs[client]->size());

	client->async_read_some(asio::buffer(m_client_bufs[client]->data(), m_client_bufs[client]->size()), m_strand.wrap([this, client](std::error_code ec, std::size_t length)
		{
			if (ec)
			{
				client->close();

				return;
			};

			// 处理数据


			_ServerReceive(client);

		}));
}

void CSimpleAsio::_Connect()
{
	if (!m_appEnable)
	{
		return;
	}

	if (m_client_ == nullptr)
	{
		return;
	}
	if (m_client_->is_open())
	{
		return;
	}
	if (!m_isServer)
	{
		m_client_->async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(m_ip.c_str()), atoi(m_port.c_str())), m_strand.wrap([this](asio::error_code ec) {
			if (ec)
			{
				m_client_->close();

			}
			else
			{
				std::string id;
				try
				{
					id = m_client_->remote_endpoint().address().to_string() + ":" + std::to_string(m_client_->remote_endpoint().port());
				}
				catch (std::exception& e)
				{
					m_client_->close();

				}

				if (!id.empty() && m_client_->is_open())
				{


					_ClientReceive();
				}
			}
			}));
	}
}

void CSimpleAsio::_ClientSend(const char* buf, const int& size, const std::string& msg)
{
	if (!m_appEnable)
	{
		return;
	}

	if (buf == nullptr || m_client_ == nullptr)
	{
		return;
	}
	if (!m_client_->is_open())
	{
		_Connect();
		return;
	}

	m_client_->async_send(asio::buffer(buf, size), m_strand.wrap([this, buf, size, msg](asio::error_code ec, std::size_t len) {
		if (ec)
		{

			m_client_->close();
			_Connect();
		}
		else
		{

		}
		}));
}

void CSimpleAsio::_ClientReceive()
{
	if (!m_appEnable)
	{
		return;
	}

	if (m_client_ == nullptr)
	{
		return;
	}
	if (!m_client_->is_open())
	{
		return;
	}

	if (m_client_bufs.find(m_client_) == m_client_bufs.end())
	{
		std::shared_ptr<std::array<char, max_buf_size>> buf = std::make_shared<std::array<char, max_buf_size>>();

		m_client_bufs[m_client_] = buf;
	}
	ZeroMemory(m_client_bufs[m_client_]->data(), m_client_bufs[m_client_]->size());


	m_client_->async_read_some(asio::buffer(m_client_bufs[m_client_]->data(), m_client_bufs[m_client_]->size()), m_strand.wrap([this](std::error_code ec, std::size_t length)
		{
			if (ec)
			{
				m_client_->close();
				return;
			};			
			_ClientReceive();

		}));
}

