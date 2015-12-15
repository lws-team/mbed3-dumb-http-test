#include "mbed-drivers/mbed.h"
#include "sal-iface-eth/EthernetInterface.h"
#include "sockets/TCPListener.h"
#include "sal-stack-lwip/lwipv4_init.h"

namespace {
	const int SERVER_PORT = 80;
}
using namespace mbed::Sockets::v0;

class connection {
public:
	connection():
		ts(NULL),
		remaining(0)
	{
	}
	void onError(Socket *s, socket_error_t err)
	{
		(void) s;
		printf("Socket Error: %s (%d)\r\n", socket_strerror(err), err);
		if (ts)
			ts->close();
	}

	int send_some(void) {
		socket_error_t err;

		err = ts->send("aha12345", 8);
		if (err != SOCKET_ERROR_NONE) {
			onError(ts, err);
		}
		
		return 0;
	}

	void onRX(Socket *s) {
		socket_error_t err;
		size_t size = sizeof(buffer);
		int n;

		err = s->recv(buffer, &size);
		n = s->error_check(err);
		if (n) {
			printf("%s: error %d\r\n", __func__, n);
			s->close();
			return;
		}
		
		send_some();
	}

	void onDisconnect(TCPStream *s) {
		if (s) {
			delete s;
			delete this;
		}
	}
	void onSent(Socket *s, uint16_t len) {
		(void)s;
		(void)len;
		send_some();
	}

public:
	TCPStream *ts;
	const unsigned char *pos;
	unsigned int remaining;
	char buffer[4096];
};


class listener {
public:
	listener():
		srv(SOCKET_STACK_LWIP_IPV4)
	{
		srv.setOnError(TCPStream::ErrorHandler_t(this, &listener::onError));
	}

	void start(const uint16_t port)
	{
		socket_error_t err = srv.open(SOCKET_AF_INET4);

		if (srv.error_check(err))
			return;
		
		err = srv.bind("0.0.0.0", port);
		if (srv.error_check(err))
			return;
		err = srv.start_listening(TCPListener::IncomingHandler_t(this,
					  &listener::onIncoming));
		srv.error_check(err);
	}

protected:
	void onError(Socket *s, socket_error_t err)
	{
		(void) s;
		printf("Socket Error: %s (%d)\r\n", socket_strerror(err), err);
		if (s)
			s->close();
	}

	void onIncoming(TCPListener *s, void *impl)
	{
		connection *conn;
		if (!impl) {
			onError(s, SOCKET_ERROR_NULL_PTR);
			return;
		}

		conn = new connection;
		conn->ts = srv.accept(impl);
		if (!conn->ts) {
			onError(s, SOCKET_ERROR_BAD_ALLOC);
			return;
		}
	
		conn->ts->setNagle(0);
	
		conn->ts->setOnError(TCPStream::ErrorHandler_t(conn, &connection::onError));
		conn->ts->setOnDisconnect(TCPStream::DisconnectHandler_t(conn,
						&connection::onDisconnect));
		conn->ts->setOnSent(Socket::SentHandler_t(conn, &connection::onSent));
		conn->ts->setOnReadable(TCPStream::ReadableHandler_t(conn, &connection::onRX));
	}

protected:
	TCPListener srv;
};

EthernetInterface eth;
listener *srv;

static void blinky(void) {
    static DigitalOut led(LED1);

    led = !led;
}

void app_start(int argc, char *argv[])
{
	static Serial pc(USBTX, USBRX);

	(void) argc;
	(void) argv;
	
	pc.baud(115200);
	printf("\r\n\r\nStarting on port 80...\r\n");
	eth.init(); // Use DHCP
	eth.connect();
	lwipv4_socket_init();
	
	srv = new listener;
	mbed::util::FunctionPointer1<void, uint16_t> fp(srv, &listener::start);
	minar::Scheduler::postCallback(fp.bind(SERVER_PORT));

	minar::Scheduler::postCallback(blinky).period(minar::milliseconds(500));
}
