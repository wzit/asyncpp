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

	/*********************��Ӽ����˿�*********************/
	/**
	���һ�������߳��飬���ڸ߲�������
	��ᴴ��һ��ȫ�ּ����߳�ListenThread��
	������һ�����ڴ���client��Ϣ���߳��飬��ʼ��client_thread_num��ClientThread
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
	���һ�������߳��飬���ڸ߲�������
	��ᴴ��һ��ȫ�ּ����߳�ListenThread��
	accept������clien�ᱻ����client_thread_poll_id�߳����д���
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
	��ȫ���߳�global_net_thread_id���һ�������˿�
	�ü����˿��ϵ��¼����Լ�accept�����������ϵ��¼���ȫ���ɸ��̴߳���
	@return error code
	*/
	bool add_listener(thread_id_t global_net_thread_id,
		const char* ip, uint16_t port)
	{
		return add_listener(ip, port,
			global_net_thread_id, -1, global_net_thread_id);
	}

	/**
	��ȫ���߳�global_net_thread_id���һ�������˿�
	����������client������client_thread_pool_id�߳����client_thread_id�̴߳���
	client_thread_pool_id=-1��ʾ����ȫ���߳�client_thread_id����
	client_thread_id=-1��ʾ�Զ�ѡ��client_thread_pool_id�е�ĳ���̴߳���
	@return true��ʾ�����ͳɹ�
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

	/*********************����������Ӷ˿�*********************/
	/**
	���һ��ȫ�������߳��������ӷ�����
	�������ɸ��̵߳����������ڸ�����������
	@return global_thread_id
	���������ǳɹ����������ʧ�ܣ��߳̽�����idle״̬�����ٴ��������
	*/
	template<typename ConnectThread>
	thread_id_t add_connector(const char* host, uint16_t port)
	{
		thread_id_t id = add_thread<ConnectThread>(-1);
		add_connector(host, port, -1, id);
		return id;
	}
	
	/*
	��net_thread_pool_id�߳����net_thread_id�߳����һ����������
	net_thread_pool_id=-1��ʾ��global_net_thread_id���һ����������
	net_thread_id=-1��ʾ�Զ�ѡ��
	@return true��ʾ�����ͳɹ�
	        ʧ��ԭ��xxx_id���Ϸ���Ŀ���߳���Ϣ������
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

	/************************����߳���************************/
	/**
	����һ���߳��飬��ʼ��thread_num��Thread�߳�
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
	��thread_pool_id�߳��������һ��Thread�߳�
	thread_pool_id=-1��ʾ��ӵ�ȫ���߳�����
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

	/********************�߳�ͬ��********************/
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
