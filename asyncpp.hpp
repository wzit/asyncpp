#ifndef _ASYNCPP_HPP_
#define _ASYNCPP_HPP_

#include "threads.hpp"

namespace asyncpp
{

class AsyncFrame
{
private:
	std::vector<ThreadPool*> m_thread_pools;
	std::unordered_map<uint64_t, MsgContext*> m_ctxs;
	std::mutex m_ctxs_mtx;
	volatile bool m_end;
#ifdef _WIN32
public:
	static HANDLE m_iocp;
#endif
public:
	AsyncFrame();
	~AsyncFrame();
	AsyncFrame(const AsyncFrame&) = delete;
	AsyncFrame& operator=(const AsyncFrame&) = delete;

	bool end()const{return m_end;}

	/*********************添加监听*********************/
	/**
	添加一个监听线程组，用于高并发场景
	这会创建一个全局监听线程ListenThread，
	并创建一个用于处理client消息的线程组，初始有client_thread_num个ClientThread
	@return <success/fail, listener thread id, client thread pool id>
	*/
	template<typename ListenThread, typename ClientThread>
	std::tuple<bool, thread_id_t, thread_pool_id_t> add_listener(
		const char* ip, uint16_t port, uint8_t client_thread_num)
	{
		thread_id_t listen_id = add_thread<ListenThread>(0);
		thread_pool_id_t client_pool_id = add_thread_pool<ClientThread>(client_thread_num);
		const auto& ret = add_listener<ListenThread>(ip, port, client_pool_id);
		return std::tuple<bool, thread_id_t, thread_pool_id_t>(ret.second, ret.first, listen_id);
	}

	/**
	添加一个监听线程组，用于高并发场景
	这会创建一个全局监听线程ListenThread，
	accept上来的clien会被发给client_thread_pool_id线程组中处理
	@return <success/fail, listener thread id>
	*/
	template<typename ListenThread>
	std::pair<bool, thread_id_t> add_listener(
		const char* ip, uint16_t port,
		thread_pool_id_t client_thread_pool_id)
	{
		thread_id_t listen_id = add_thread<ListenThread>(0);
		bool ret = add_listener(ip, port, listen_id, client_thread_pool_id, -1);
		return std::pair<bool, thread_id_t>(ret, listen_id);
	}

	/**
	向全局线程global_net_thread_id添加一个监听端口
	该监听端口上的事件，以及accept上来的连接上的事件，全部由该线程处理
	@return success/fail
	*/
	bool add_listener(thread_id_t global_net_thread_id,
		const char* ip, uint16_t port,
		BaseThread* sender = nullptr, int32_t seq = 0)
	{
		return add_listener(ip, port,
			global_net_thread_id, 0, global_net_thread_id, sender, seq);
	}

	/**
	向全局线程global_net_thread_id添加一个监听端口
	连接上来的client将交由client_thread_pool_id线程组的client_thread_id线程处理
	client_thread_id=INVALID_THREAD_ID表示自动选择client_thread_pool_id中的某个线程处理
	@return true表示请求发送成功
	*/
	bool add_listener(const char* ip, uint16_t port,
		thread_id_t global_net_thread_id,
		thread_pool_id_t client_thread_pool_id, thread_id_t client_thread_id,
		BaseThread* sender = nullptr, int32_t seq = 0)
	{
		ThreadMsg msg;
		auto ctx = new AddListenerCtx;
		msg.m_type = NET_LISTEN_ADDR_REQ;
		if (sender != nullptr)
		{
			msg.m_src_thread_id = sender->get_id();
			msg.m_src_thread_pool_id = sender->get_thread_pool_id();
		}
		msg.m_dst_thread_id = global_net_thread_id;
		msg.m_dst_thread_pool_id = 0;
		msg.m_buf_len = static_cast<uint32_t>(strlen(ip));
		msg.m_buf = new char[msg.m_buf_len + 1];
		memcpy(msg.m_buf, ip, msg.m_buf_len + 1);
		msg.m_buf_type = MsgBufferType::NEW;
		ctx->m_port = port;
		ctx->m_client_thread_pool_id = client_thread_pool_id;
		ctx->m_client_thread_id = client_thread_id;
		ctx->m_seq = seq;
		msg.m_ctx.obj = ctx;
		msg.m_ctx_type = MsgContextType::OBJECT;

		return send_thread_msg(std::move(msg));
	}

	/*********************添加连接*********************/
	/**
	添加一个全局网络线程用于连接服务器
	该连接由该线程单独管理，用于高吞吐量场景
	@return global_thread_id
	本调用总是成功，如果连接失败，线程将处于idle状态，可再次添加连接
	*/
	template<typename ConnectThread>
	thread_id_t add_connector(const char* host, uint16_t port)
	{
		thread_id_t id = add_thread<ConnectThread>(0);
		add_connector(host, port, 0, id);
		return id;
	}
	
	/**
	向net_thread_pool_id线程组的net_thread_id线程添加一个网络连接
	net_thread_pool_id=0表示向global_net_thread_id添加一个网络连接
	net_thread_id=INVALID_THREAD_ID表示自动选择
	@return true表示请求发送成功
	        失败原因：xxx_id不合法，目标线程消息队列满
	*/
	bool add_connector(const char* host, uint16_t port,
		thread_pool_id_t net_thread_pool_id, thread_id_t net_thread_id,
		BaseThread* sender = nullptr, int32_t seq = 0)
	{
		ThreadMsg msg;
		auto ctx = new AddConnectorCtx;
		msg.m_type = NET_CONNECT_HOST_REQ;
		if (sender != nullptr)
		{
			msg.m_src_thread_id = sender->get_id();
			msg.m_src_thread_pool_id = sender->get_thread_pool_id();
		}
		msg.m_dst_thread_id = net_thread_id;
		msg.m_dst_thread_pool_id = net_thread_pool_id;
		msg.m_buf_len = static_cast<uint32_t>(strlen(host));
		msg.m_buf = new char[msg.m_buf_len + 1];
		memcpy(msg.m_buf, host, msg.m_buf_len + 1);
		msg.m_buf_type = MsgBufferType::NEW;
		ctx->m_seq = seq;
		ctx->m_port = port;
		msg.m_ctx.obj = ctx;
		msg.m_ctx_type = MsgContextType::OBJECT;
		
		return send_thread_msg(std::move(msg));
	}

	/************************添加线程组************************/
	/**
	创建一个线程组，初始有thread_num个Thread线程
	@return t_pool_id
	*/
	template<typename Thread>
	thread_pool_id_t add_thread_pool(uint8_t thread_num)
	{
		thread_pool_id_t id = m_thread_pools.size();
		ThreadPool* t_pool = new ThreadPool(this, id);
		m_thread_pools.push_back(t_pool);
		for (uint32_t i = 0; i < thread_num; ++i)
		{
			t_pool->add_thread(new Thread());
		}
		return id;
	}

	/**
	向thread_pool_id线程组中添加一个Thread线程
	@return t_id
	*/
	template<typename Thread>
	thread_id_t add_thread(thread_pool_id_t t_pool_id)
	{
		BaseThread* new_thread = new Thread();
		return add_thread(t_pool_id, new_thread);
	}

	thread_id_t add_thread(thread_pool_id_t t_pool_id, BaseThread* new_thread)
	{
		return m_thread_pools[t_pool_id]->add_thread(new_thread);
	}

	/********************线程同步********************/
	/*
	 force_receiver_thread 如果为false，当目标线程消息队列满时尝试将该消息投递到目标线程组
	 如果发送成功，接管msg；发送失败则msg保持不变
	*/
	bool send_thread_msg(ThreadMsg&& msg, bool force_receiver_thread = false)
	{
		bool ret =  m_thread_pools[msg.m_dst_thread_pool_id]->push_msg(
			std::move(msg),
			msg.m_dst_thread_pool_id == 0 ? true : force_receiver_thread);
		if (!ret)
		{
			_WARNLOG(logger, "send thread msg from %hu:%hu to %hu:%hu fail, type:%u",
				msg.m_src_thread_pool_id, msg.m_src_thread_id,
				msg.m_dst_thread_pool_id, msg.m_src_thread_id, msg.m_type);
		}
		else
		{
			_TRACELOG(logger, "send thread msg from %hu:%hu to %hu:%hu, type:%u, dst queued msg cnt:%u",
				msg.m_src_thread_pool_id, msg.m_src_thread_id,
				msg.m_dst_thread_pool_id, msg.m_src_thread_id, msg.m_type,
				get_queued_msg_number(msg.m_dst_thread_pool_id, msg.m_dst_thread_id));
		}
		return ret;
	}

	/*
	 本函数调用后接管buf和ctx，请勿再继续使用
	*/
	bool send_thread_msg(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, MsgCtx ctx, MsgContextType ctx_type,
		thread_pool_id_t receiver_thread_pool, thread_id_t receiver_thread,
		const BaseThread* sender, bool force_receiver_thread = false)
	{
		if (sender != nullptr)
		{
			return send_thread_msg(ThreadMsg(ctx, msg_type,
				buf_len, buf, buf_type,
				ctx_type, sender->get_id(), receiver_thread,
				sender->get_thread_pool_id(), receiver_thread_pool),
				force_receiver_thread);
		}
		else
		{
			return send_thread_msg(ThreadMsg(ctx, msg_type,
				buf_len, buf, buf_type,
				ctx_type, INVALID_THREAD_ID, receiver_thread,
				INVALID_THREAD_POOL_ID, receiver_thread_pool),
				force_receiver_thread);
		}
	}

	/*
	 如果发送成功，接管buf和ctx；发送失败则传入的信息保持不变
	*/
	bool send_thread_msg2(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, MsgCtx ctx, MsgContextType ctx_type,
		thread_pool_id_t receiver_thread_pool, thread_id_t receiver_thread,
		const BaseThread* sender, bool force_receiver_thread = false)
	{
		if (sender != nullptr)
		{
			ThreadMsg tmsg(ctx, msg_type, buf_len, buf, buf_type,
				ctx_type, sender->get_id(), receiver_thread,
				sender->get_thread_pool_id(), receiver_thread_pool);
			if (send_thread_msg(std::move(tmsg), force_receiver_thread))
				return true;
			else
			{
				tmsg.detach();
				return false;
			}
		}
		else
		{
			ThreadMsg tmsg(ctx, msg_type, buf_len, buf, buf_type,
				ctx_type, INVALID_THREAD_ID, receiver_thread,
				INVALID_THREAD_POOL_ID, receiver_thread_pool);
			if (send_thread_msg(std::move(tmsg), force_receiver_thread))
				return true;
			else
			{
				tmsg.detach();
				return false;
			}
		}
	}

	/*
	 本函数调用后接管buf，请勿再继续使用
	*/
	bool send_thread_msg(int32_t msg_type,
		char* buf, uint32_t buf_len, MsgBufferType buf_type,
		thread_pool_id_t receiver_thread_pool, thread_id_t receiver_thread,
		const BaseThread* sender, bool force_receiver_thread = false)
	{
		if (sender != nullptr)
		{
			return send_thread_msg(ThreadMsg({0}, msg_type,
				buf_len, buf, buf_type,
				MsgContextType::STATIC, sender->get_id(), receiver_thread,
				sender->get_thread_pool_id(), receiver_thread_pool),
				force_receiver_thread);
		}
		else
		{
			return send_thread_msg(ThreadMsg({0}, msg_type,
				buf_len, buf, buf_type, MsgContextType::STATIC,
				INVALID_THREAD_ID, receiver_thread,
				INVALID_THREAD_POOL_ID, receiver_thread_pool),
				force_receiver_thread);
		}
	}

	/*
	 如果发送成功，接管buf；发送失败则传入的信息保持不变
	*/
	bool send_thread_msg_no_ctx_2(int32_t msg_type,
		char* buf, uint32_t buf_len, MsgBufferType buf_type,
		thread_pool_id_t receiver_thread_pool, thread_id_t receiver_thread,
		const BaseThread* sender, bool force_receiver_thread = false)
	{
		if (sender != nullptr)
		{
			ThreadMsg tmsg({0}, msg_type, buf_len, buf, buf_type,
				MsgContextType::STATIC, sender->get_id(), receiver_thread,
				sender->get_thread_pool_id(), receiver_thread_pool);
			if (send_thread_msg(std::move(tmsg), force_receiver_thread))
				return true;
			else
			{
				tmsg.detach_buf();
				return false;
			}
		}
		else
		{
			ThreadMsg tmsg({0}, msg_type,
				buf_len, buf, buf_type, MsgContextType::STATIC,
				INVALID_THREAD_ID, receiver_thread,
				INVALID_THREAD_POOL_ID, receiver_thread_pool);
			if (send_thread_msg(std::move(tmsg), force_receiver_thread))
				return true;
			else
			{
				tmsg.detach_buf();
				return false;
			}
		}
	}

	/*
	 本函数调用后接管buf和ctx，请勿再继续使用
	*/
	bool send_resp_msg(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, MsgCtx ctx, MsgContextType ctx_type,
		const ThreadMsg& req, const BaseThread* sender)
	{
		if (req.m_src_thread_pool_id != INVALID_THREAD_POOL_ID
			&& req.m_src_thread_id != INVALID_THREAD_ID)
		{
			return send_thread_msg(msg_type,
				buf, buf_len, buf_type, ctx, ctx_type,
				req.m_src_thread_pool_id, req.m_src_thread_id, sender, true);
		}
		else
		{
			_WARNLOG(logger, "invalid thread msg, type:%u", msg_type);
			free_buffer(buf, buf_type);
			free_context(ctx, ctx_type);
			return false;
		}
	}

	/*
	 如果发送成功，接管buf和ctx；发送失败则传入的信息保持不变
	*/
	bool send_resp_msg2(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, MsgCtx ctx, MsgContextType ctx_type,
		const ThreadMsg& req, const BaseThread* sender)
	{
		if (req.m_src_thread_pool_id != INVALID_THREAD_POOL_ID
			&& req.m_src_thread_id != INVALID_THREAD_ID)
		{
			return send_thread_msg2(msg_type,
				buf, buf_len, buf_type, ctx, ctx_type,
				req.m_src_thread_pool_id, req.m_src_thread_id, sender, true);
		}
		else
		{
			_WARNLOG(logger, "invalid thread msg, type:%u", msg_type);
			return false;
		}
	}

	/*
	 本函数调用后接管buf，请勿再继续使用
	*/
	bool send_resp_msg(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, const ThreadMsg& req, const BaseThread* sender)
	{
		return send_resp_msg(msg_type, buf, buf_len, buf_type,
					{0}, MsgContextType::STATIC, req, sender);
	}

	/*
	 如果发送成功，接管buf；发送失败则传入的信息保持不变
	*/
	bool send_resp_msg_no_ctx_2(int32_t msg_type, char* buf, uint32_t buf_len,
		MsgBufferType buf_type, const ThreadMsg& req, const BaseThread* sender)
	{
		return send_resp_msg2(msg_type, buf, buf_len, buf_type,
					{0}, MsgContextType::STATIC, req, sender);
	}

	bool is_msg_queue_full(thread_pool_id_t t_pool_id, thread_id_t t_id)
	{
		return t_id == INVALID_THREAD_ID
			? is_msg_queue_full(t_pool_id)
			: get_thread(t_pool_id, t_id)->full();
	}
	uint32_t get_queued_msg_number(thread_pool_id_t t_pool_id, thread_id_t t_id)
	{
		return t_id == INVALID_THREAD_ID
			? get_queued_msg_number(t_pool_id)
			: get_thread(t_pool_id, t_id)->get_queued_msg_number();
	}
	bool is_msg_queue_full(thread_pool_id_t t_pool_id)
	{
		return get_thread_pool(t_pool_id)->full();
	}
	uint32_t get_queued_msg_number(thread_pool_id_t t_pool_id)
	{
		return get_thread_pool(t_pool_id)->get_queued_msg_number();
	}

	/*****************************server manager*****************************/
	ThreadPool* get_thread_pool(thread_pool_id_t t_pool_id)
	{
		return m_thread_pools[t_pool_id];
	}
	BaseThread* get_thread(thread_pool_id_t t_pool_id, thread_id_t t_id)
	{
		return m_thread_pools[t_pool_id]->operator[](t_id);
	}

	void start_thread(BaseThread* t);
	void start_thread(thread_pool_id_t t_pool_id, thread_id_t t_id);
	void start(); //block
	void stop();

public:
	void add_ctx(uint64_t seq, MsgContext* ctx)
	{
		std::lock_guard<std::mutex> _lock(m_ctxs_mtx);
		m_ctxs.insert(std::make_pair(seq, ctx));
	}
	MsgContext* del_ctx(uint64_t seq)
	{
		std::lock_guard<std::mutex> _lock(m_ctxs_mtx);
		const auto& it = m_ctxs.find(seq);
		if (it != m_ctxs.end())
		{
			MsgContext* p = it->second;
			m_ctxs.erase(it);
			return p;
		}
		else return nullptr;
	}
};

} //end of namespace asyncpp

#endif
