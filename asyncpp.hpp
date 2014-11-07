#ifndef _ASYNCPP_HPP_
#define _ASYNCPP_HPP_

#include "threads.hpp"

namespace asyncpp
{

class AsyncFrame
{
private:
	std::vector<ThreadPool*> m_thread_pools;
	std::vector<BaseThread*> m_global_threads;
#ifdef _WIN32
public:
	static HANDLE m_iocp;
#endif
public:
#ifdef _WIN32
	AsyncFrame();
	~AsyncFrame();
#else
	AsyncFrame() = default;
	~AsyncFrame(){for(auto t:m_thread_pools)delete t;};
#endif
	AsyncFrame(const AsyncFrame&) = delete;
	AsyncFrame& operator=(const AsyncFrame&) = delete;

	/*********************添加监听端口*********************/
	/**
	添加一个监听线程组，用于高并发场景
	这会创建一个全局监听线程ListenThread，
	并创建一个用于处理client消息的线程组，初始有client_thread_num个ClientThread
	@return <error code, listener thread id, client thread pool id>
	*/
	template<typename ListenThread, typename ClientThread>
	std::tuple<bool, thread_id_t, thread_pool_id_t> add_listener(
		const char* ip, uint16_t port, uint8_t client_thread_num)
	{
		thread_id_t listen_id = add_thread<ListenThread>(-1);
		thread_pool_id_t client_pool_id = add_thread_pool<ClientThread>(client_thread_num);
		const auto& ret = add_listener<ListenThread>(ip, port, client_pool_id);
		return std::tuple<bool, thread_id_t, thread_pool_id_t>(ret.second, ret.first, listen_id);
	}

	/**
	添加一个监听线程组，用于高并发场景
	这会创建一个全局监听线程ListenThread，
	accept上来的clien会被发给client_thread_poll_id线程组中处理
	@return <listener thread id, success/fail>
	*/
	template<typename ListenThread>
	std::pair<thread_id_t, bool> add_listener(
		const char* ip, uint16_t port,
		thread_pool_id_t client_thread_poll_id)
	{
		thread_id_t listen_id = add_thread<ListenThread>(-1);
		bool ret = add_listener(ip, port, listen_id, client_thread_poll_id, -1);
		return std::pair<thread_id_t, bool>(listen_id, ret);
	}

	/**
	向全局线程global_net_thread_id添加一个监听端口
	该监听端口上的事件，以及accept上来的连接上的事件，全部由该线程处理
	@return error code
	*/
	bool add_listener(thread_id_t global_net_thread_id,
		const char* ip, uint16_t port)
	{
		return add_listener(ip, port,
			global_net_thread_id, -1, global_net_thread_id);
	}

	/**
	向全局线程global_net_thread_id添加一个监听端口
	连接上来的client将交由client_thread_pool_id线程组的client_thread_id线程处理
	client_thread_pool_id=-1表示交由全局线程client_thread_id处理
	client_thread_id=-1表示自动选择client_thread_pool_id中的某个线程处理
	@return true表示请求发送成功
	*/
	bool add_listener(const char* ip, uint16_t port,
		thread_id_t global_net_thread_id,
		thread_pool_id_t client_thread_pool_id, thread_id_t client_thread_id)
	{
		ThreadMsg msg;
		msg.m_type = NET_LISTEN_ADDR_REQ;
		msg.m_dst_thread_id = global_net_thread_id;
		msg.m_buf_len = strlen(ip);
		msg.m_buf = new char[msg.m_buf_len + 1];
		memcpy(msg.m_buf, ip, msg.m_buf_len + 1);
		msg.m_buf_type = MsgBufferType::NEW;
		msg.m_ctx.i64 = (static_cast<uint64_t>(port) << 32)
			| (static_cast<uint64_t>(client_thread_pool_id) << 16)
			| static_cast<uint64_t>(client_thread_id);

		return send_thread_msg(std::move(msg));
	}

	/*********************添加主动连接端口*********************/
	/**
	添加一个全局网络线程用于连接服务器
	该连接由该线程单独管理，用于高吞吐量场景
	@return global_thread_id
	本调用总是成功，如果连接失败，线程将处于idle状态，可再次添加连接
	*/
	template<typename ConnectThread>
	thread_id_t add_connector(const char* host, uint16_t port)
	{
		thread_id_t id = add_thread<ConnectThread>(-1);
		add_connector(host, port, -1, id);
		return id;
	}
	
	/*
	向net_thread_pool_id线程组的net_thread_id线程添加一个网络连接
	net_thread_pool_id=-1表示向global_net_thread_id添加一个网络连接
	net_thread_id=-1表示自动选择
	@return true表示请求发送成功
	        失败原因：xxx_id不合法，目标线程消息队列满
	*/
	bool add_connector(const char* host, uint16_t port,
		thread_pool_id_t net_thread_pool_id, thread_id_t net_thread_id)
	{
		ThreadMsg msg;
		msg.m_type = NET_CONNECT_HOST_REQ;
		msg.m_dst_thread_id = net_thread_id;
		msg.m_dst_thread_pool_id = net_thread_pool_id;
		msg.m_buf_len = strlen(host);
		msg.m_buf = new char[msg.m_buf_len + 1];
		memcpy(msg.m_buf, host, msg.m_buf_len + 1);
		msg.m_buf_type = MsgBufferType::NEW;
		msg.m_ctx.i64 = static_cast<uint64_t>(port);
		
		return send_thread_msg(std::move(msg));
	}

	/************************添加线程组************************/
	/**
	创建一个线程组，初始有thread_num个Thread线程
	@return thread_pool_id
	*/
	template<typename Thread>
	thread_pool_id_t add_thread_pool(uint8_t thread_num)
	{
		thread_pool_id_t id = m_thread_pools.size();
		ThreadPool* thread_pool = new ThreadPool(this, id);
		for (uint32_t i = 0; i < thread_num; ++i)
		{
			thread_pool->add_thread(new Thread());
		}
		return id;
	}

	/**
	向thread_pool_id线程组中添加一个Thread线程
	thread_pool_id=-1表示添加到全局线程组中
	@return thread_id
	*/
	template<typename Thread>
	thread_id_t add_thread(thread_pool_id_t thread_pool_id)
	{
		BaseThread* new_thread = new Thread();
		return add_thread(thread_pool_id, new_thread);
	}

	thread_id_t add_thread(thread_pool_id_t thread_pool_id, BaseThread* new_thread)
	{
		if (thread_pool_id >= 0)
		{
			m_thread_pools[thread_pool_id]->add_thread(new_thread);
		}
		else
		{
			new_thread->set_id(m_global_threads.size());
			m_global_threads.push_back(new_thread);
		}
		return new_thread->get_id();
	}

	/********************线程同步********************/
	bool send_thread_msg(ThreadMsg&& msg)
	{
		if (msg.m_dst_thread_pool_id >= 0)
		{
			return m_thread_pools[msg.m_dst_thread_pool_id]->push_msg(std::move(msg));
		}
		else
		{
			if (msg.m_dst_thread_id >= 0)
			{
				return m_global_threads[msg.m_dst_thread_id]->push_msg(std::move(msg));
			}
			else return false;
		}
	}
	bool send_thread_msg(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, MsgCtx* ctx, MsgContextType ctx_type,
		thread_pool_id_t receiver_thread_pool, thread_id_t receiver_thread,
		const BaseThread* sender, bool force_receiver_thread = false)
	{
		send_thread_msg(ThreadMsg(ctx, msg_type, buf_len, buf, buf_type,
			ctx_type, sender->get_id(), receiver_thread,
			sender->get_thread_pool()->get_id(), receiver_thread_pool));
	}

	bool is_msg_queue_full(thread_pool_id_t thread_pool_id, thread_id_t thread_id)
	{
		return get_thread(thread_pool_id, thread_id)->full();
	}
	bool is_msg_queue_full(thread_pool_id_t thread_pool_id)
	{
		return get_thread_pool(thread_pool_id)->full();
	}

	/*****************************server manager*****************************/
	ThreadPool* get_thread_pool(thread_pool_id_t thread_pool_id)
	{
		return thread_pool_id >= 0 ? m_thread_pools[thread_pool_id] : nullptr;
	}
	BaseThread* get_thread(thread_pool_id_t thread_pool_id, thread_id_t thread_id)
	{
		return thread_pool_id >= 0 ?
			m_thread_pools[thread_pool_id]->operator[](thread_id) :
			m_global_threads[thread_id];
	}

	void start_thread(BaseThread* t);
	void start(); //block
};

} //end of namespace asyncpp

#endif
