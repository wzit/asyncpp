#include "lib/asyncpp/asyncpp.hpp"

using namespace std;
using namespace asyncpp;

AsyncFrame g_asynf;
thread_id_t g_client_work_thread;
thread_id_t g_client_net_thread;

/********************server********************/
class ServerEchoThread : public MultiplexNetThread<SelSelector>
{
public:
	virtual void process_net_msg(NetConnect* conn) override
	{
		/* // echo
		char* buf = (char*)malloc(conn->m_recv_len);
		memcpy(buf, conn->m_recv_buf, conn->m_recv_len);
		send(conn, buf, conn->m_recv_len, MsgBufferType::MALLOC);*/
		char* buf = (char*)malloc(256);
		int n = sprintf(buf, "HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html; charset=UTF-8\r\n"
			"Connection: close\r\n"
			"Cache-Control: no-cache, must-revalidate\r\n\r\n"
			"Hello, World!\r\n");
		send(conn, buf, n, MsgBufferType::MALLOC);
		close(conn);
	}
};


/*******************client*****************/
void t1(uint32_t timer_id, uint64_t ctx)
{
	//创建一个连接
	g_asynf.add_connector("127.0.0.1", 22873, 0, g_client_net_thread,
		g_asynf.get_thread(0, g_client_work_thread));
	printf("hello %llu\n", ctx);
}

class ClientNetThread : public MultiplexNetThread<SelSelector>
{
protected:
	virtual int32_t frame(NetConnect* conn) override
	{
		//本函数返回一个包的长度
		return conn->m_recv_len;
	}
public:
	virtual void process_net_msg(NetConnect* conn) override
	{
		//将服务器的回包投递给工作线程
		char* buf = new char[conn->m_recv_len];
		memcpy(buf, conn->m_recv_buf, conn->m_recv_len);
		get_asynframe()->send_thread_msg(100,
			buf, conn->m_recv_len, MsgBufferType::NEW,
			0, g_client_work_thread, this, true);
	}

	virtual void process_msg(ThreadMsg& msg) override
	{
		switch (msg.m_type)
		{
		case 200:
			//处理工作线程的发包请求
			send(static_cast<uint32_t>(msg.m_ctx.i64),
				msg.m_buf, msg.m_buf_len, msg.m_buf_type);
			break;
		default:
			//处理新建连接等请求
			MultiplexNetThread<SelSelector>::process_msg(msg);
		}
		free_buffer(msg.m_buf, msg.m_buf_type);
	}
};

class ClientWorkThread : public BaseThread
{
public:
	virtual void process_msg(ThreadMsg& msg) override
	{
		switch (msg.m_type)
		{
		case NET_CONNECT_HOST_RESP:
			//新建连接响应
			//向网络线程发送消息
			get_asynframe()->send_thread_msg(200, "1234567890", 10, MsgBufferType::STATIC,
				msg.m_ctx, MsgContextType::STATIC, 0, g_client_net_thread, this, true);
			break;
		case 100:
			//收到网络线程发来的消息，打印到标准输出
			printf("\n-----recv-----\n%.*s\n", msg.m_buf_len, msg.m_buf);
			add_timer(1, t1, 3);
			break;
		default:
			printf("\n-----recv error msg %u -----\n", msg.m_buf_type);
			add_timer(1, t1, 3);
			break;
		}
		free_buffer(msg.m_buf, msg.m_buf_type);
	}
};
int main()
{
	/********************server********************/
	//创建服务线程
	thread_id_t sid = g_asynf.add_thread<ServerEchoThread>(0);
	//向服务线程添加一个监听端口
	g_asynf.add_listener(sid, "0.0.0.0", 22873);

	/*******************client*****************/
	//创建客户端工作线程
	g_client_work_thread = g_asynf.add_thread<ClientWorkThread>(0);
	//创建客户端网络线程
	g_client_net_thread = g_asynf.add_thread<ClientNetThread>(0);
	//向客户端工作线程发送一条消息
	g_asynf.send_thread_msg(1234, nullptr, 0, MsgBufferType::STATIC,
		{ 0 }, MsgContextType::STATIC, 0, g_client_work_thread,
		g_asynf.get_thread(0, g_client_work_thread), true);

	/*******************开启服务******************/
	//开始所有线程，并阻塞
	g_asynf.start();
}