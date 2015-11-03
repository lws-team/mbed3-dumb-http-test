#include "mbed-drivers/mbed.h"
#include "sal-iface-eth/EthernetInterface.h"
#include "sockets/TCPListener.h"
#include "sal-stack-lwip/lwipv4_init.h"

namespace {
	const int SERVER_PORT = 80;
	const int BUFFER_SIZE = 4096;
}
using namespace mbed::Sockets::v0;

class Lws {
public:
	Lws():
		srv(SOCKET_STACK_LWIP_IPV4),
		ts(NULL)
	{
		srv.setOnError(TCPStream::ErrorHandler_t(this, &Lws::onError));
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
					  &Lws::onIncoming));
		srv.error_check(err);
	}

protected:
	void onError(Socket *s, socket_error_t err)
	{
		(void) s;
		printf("Socket Error: %s (%d)\r\n", socket_strerror(err), err);
		if (ts)
			ts->close();
	}

	void onIncoming(TCPListener *s, void *impl)
	{
//		volatile int n;

		if (!impl) {
			onError(s, SOCKET_ERROR_NULL_PTR);
			return;
		}
		ts = srv.accept(impl);
		if (!ts) {
			onError(s, SOCKET_ERROR_BAD_ALLOC);
			return;
		}

#if 0
		/* this loses the first data packet from the peer */
		for (n = 0; n < 1000; n++)
			;
#else
//		(void)n;
#endif
		
		ts->setOnError(TCPStream::ErrorHandler_t(this, &Lws::onError));
		ts->setOnReadable(TCPStream::ReadableHandler_t(this,
						&Lws::onRX));
		ts->setOnDisconnect(TCPStream::DisconnectHandler_t(this,
						&Lws::onDisconnect));
		ts->setOnSent(Socket::SentHandler_t(this, &Lws::onSent));
	}

	void onRX(Socket *s) {
		socket_error_t err;
		static const char *rsp =
			"HTTP/1.1 200 OK\r\n"
			"\r\n"
			"Ahaha... hello\r\n";
		size_t size = BUFFER_SIZE - 1;
		int n;
		
		err = s->recv(buffer, &size);
		n = s->error_check(err);
		if (!n) {
			buffer[size] = 0;
			printf("%d: %s", size, buffer);

			err = s->send(rsp, strlen(rsp));
			if (err != SOCKET_ERROR_NONE)
				onError(s, err);
		} else
			printf("%s: error %d\r\n", __func__, n);
	}

	void onDisconnect(TCPStream *s) {
		if (s)
			delete s;
	}
	void onSent(Socket *s, uint16_t len) {
		(void)s;
		(void)len;
		ts->close();
	}

protected:
	TCPListener srv;
	TCPStream *ts;
	char buffer[BUFFER_SIZE];
};

EthernetInterface eth;
Lws *srv;

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
	eth.init(); // Use DHCP
	eth.connect();
	lwipv4_socket_init();
	printf("IP: %s:%d\r\n", eth.getIPAddress(), SERVER_PORT);
	
	srv = new Lws;

	minar::Scheduler::postCallback(blinky).period(minar::milliseconds(500));

	mbed::util::FunctionPointer1<void, uint16_t> fp(srv, &Lws::start);
	minar::Scheduler::postCallback(fp.bind(SERVER_PORT));
}
