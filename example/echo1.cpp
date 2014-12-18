#include "asyncpp.hpp"

using namespace std;
using namespace asyncpp;

AsyncFrame g_asynf;
thread_id_t g_client_work_thread;
thread_id_t g_client_net_thread;

/********************server********************/
class ServerEchoThread : public MultiWaitNetThread<SelSelector>
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
	//����һ������
	g_asynf.add_connector("127.0.0.1", 22873, 0, g_client_net_thread,
		g_asynf.get_thread(0, g_client_work_thread));
	printf("hello %llu\n", ctx);
}

class ClientNetThread : public MultiWaitNetThread<SelSelector>
{
protected:
	virtual int32_t frame(NetConnect* conn) override
	{
		//����������һ�����ĳ���
		return conn->m_recv_len;
	}
public:
	virtual void process_net_msg(NetConnect* conn) override
	{
		//���������Ļذ�Ͷ�ݸ������߳�
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
			//�������̵߳ķ�������
			send(static_cast<uint32_t>(msg.m_ctx.i64),
				msg.m_buf, msg.m_buf_len, msg.m_buf_type);
			break;
		default:
			//�����½����ӵ�����
			MultiWaitNetThread<SelSelector>::process_msg(msg);
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
			//�½�������Ӧ
			//�������̷߳�����Ϣ
			get_asynframe()->send_thread_msg(200, "1234567890", 10, MsgBufferType::STATIC,
				msg.m_ctx, MsgContextType::STATIC, 0, g_client_net_thread, this, true);
			break;
		case 100:
			//�յ������̷߳�������Ϣ����ӡ����׼���
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
	//���������߳�
	thread_id_t sid = g_asynf.add_thread<ServerEchoThread>(0);
	//������߳����һ�������˿�
	g_asynf.add_listener(sid, "0.0.0.0", 22873);

	/*******************client*****************/
	//�����ͻ��˹����߳�
	g_client_work_thread = g_asynf.add_thread<ClientWorkThread>(0);
	//�����ͻ��������߳�
	g_client_net_thread = g_asynf.add_thread<ClientNetThread>(0);
	//��ͻ��˹����̷߳���һ����Ϣ
	g_asynf.send_thread_msg(1234, nullptr, 0, MsgBufferType::STATIC,
		{ 0 }, MsgContextType::STATIC, 0, g_client_work_thread,
		g_asynf.get_thread(0, g_client_work_thread), true);

	/*******************��������******************/
	//��ʼ�����̣߳�������
	g_asynf.start();
}