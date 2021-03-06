#include "internal/ServerHub.h"
#include "uWS.h"
#include "internal/WebSocket.h"
#include "Input.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <assert.h>

namespace SL {
	namespace RAT {
		class ServerHubImpl {
		public:
			std::shared_ptr<Server_Config> Config_;
			uWS::Hub h;
	
			std::atomic_int ClientCount;
			IServerDriver * IServerDriver_;
			std::function<void(const std::shared_ptr<IWebSocket>&)> onConnection;
			std::function<void(const IWebSocket&, const char*, size_t)> onMessage;
			std::function<void(const IWebSocket&, int, char*, size_t)> onDisconnection;

			ServerHubImpl(IServerDriver* r, std::shared_ptr<Server_Config> config): IServerDriver_(r), Config_(config) {

				ClientCount = 0;

				h.onConnection([&](uWS::WebSocket<uWS::SERVER>* ws, uWS::HttpRequest req) {
					static int counter = 0;
					if (ClientCount + 1 > Config_->MaxNumConnections) {
						char msg[] = "Closing due to max number of connections!";
						ws->close(1000, msg, sizeof(msg));
					}
					else {

						int t = counter++ % Config_->MaxWebSocketThreads;
						SL_RAT_LOG(Logging_Levels::INFO_log_level, "Transfering connection to thread " << t);
						ws->setUserData(new SocketStats());

						IServerDriver_->onConnection(std::make_shared<WebSocket<uWS::WebSocket<uWS::SERVER>*>>(ws, (std::mutex*)h.getDefaultGroup<uWS::SERVER>().getUserData()));

						ClientCount += 1;
					}
				});


				// register our events

				h.getDefaultGroup<uWS::SERVER>().setUserData(new std::mutex);

				h.onDisconnection([&](uWS::WebSocket<uWS::SERVER>* ws, int code, char *message, size_t length) {
					SL_RAT_LOG(Logging_Levels::INFO_log_level, "onDisconnection  ");
					WebSocket<uWS::WebSocket<uWS::SERVER>*> sock(ws, (std::mutex*)h.getDefaultGroup<uWS::SERVER>().getUserData());
					ClientCount -= 1;
					IServerDriver_->onDisconnection(sock, code, message, length);
					delete (SocketStats*)ws->getUserData();
				});
				h.onMessage([&](uWS::WebSocket<uWS::SERVER>* ws, char *message, size_t length, uWS::OpCode code) {

					auto s = (SocketStats*)ws->getUserData();
					s->TotalBytesReceived += length;
					s->TotalPacketReceived += 1;

					WebSocket<uWS::WebSocket<uWS::SERVER>*> sock(ws, (std::mutex*)h.getDefaultGroup<uWS::SERVER>().getUserData());
					auto pactype = PACKET_TYPES::INVALID;
					assert(length >= sizeof(pactype));

					pactype = *reinterpret_cast<const PACKET_TYPES*>(message);
					length -= sizeof(pactype);
					message += sizeof(pactype);
					switch (pactype) {
					case PACKET_TYPES::MOUSEEVENT:
						assert(length == sizeof(MouseEvent));
						IServerDriver_->onReceive_Mouse(reinterpret_cast<const MouseEvent*>(message));
						break;
					case PACKET_TYPES::KEYEVENT:
						assert(length == sizeof(KeyEvent));
						IServerDriver_->onReceive_Key(reinterpret_cast<const KeyEvent*>(message));
						break;
					case PACKET_TYPES::CLIPBOARDTEXTEVENT:
						IServerDriver_->onReceive_ClipboardText(message, length);
						break;
					default:
						IServerDriver_->onMessage(sock, message - sizeof(pactype), length + sizeof(pactype));
						break;
					}
				});

				//uS::TLS::Context c = uS::TLS::createContext(config->PathTo_Public_Certficate, config->PathTo_Private_Key, config->PasswordToPrivateKey);
				h.listen(Config_->WebSocketTLSLPort, nullptr, 0, nullptr);
			}
			~ServerHubImpl() {
				auto uv_async_callback = [](uv_async_t* handle) {
					auto uv_walk_callback = [](uv_handle_t* handle, void* /*arg*/) {
						if (!uv_is_closing(handle))
							uv_close(handle, nullptr);
					};

					auto loop = handle->loop;
					auto hub = (uWS::Hub *)handle->data;
					hub->getDefaultGroup<true>().close();

					uv_stop(loop);
					uv_walk(loop, uv_walk_callback, nullptr);
					uv_run(loop, UV_RUN_DEFAULT);
					uv_loop_close(loop);
				};

				auto loop = h.getLoop();

				uv_async_t asyncHandle;
				asyncHandle.data = &h;
				uv_async_init(loop, &asyncHandle, uv_async_callback);
				uv_async_send(&asyncHandle);
				delete (std::mutex*)h.getDefaultGroup<uWS::SERVER>().getUserData();

			}
	
			void Broadcast(char * data, size_t len) {
				if (ClientCount <= 0) return;//no one here to send to

				std::lock_guard<std::mutex> lock(*(std::mutex*)h.getDefaultGroup<uWS::SERVER>().getUserData());
				// uwebsockets broadcast code below
				auto preparedMessage = uWS::WebSocket<uWS::SERVER>::prepareMessage(data, len, uWS::OpCode::BINARY, false);
				h.getDefaultGroup<uWS::SERVER>().forEach([preparedMessage, len](uWS::WebSocket<uWS::SERVER>* ws) {
					ws->sendPrepared(preparedMessage);
					auto s = (SocketStats*)ws->getUserData();
					s->TotalBytesSent += len;
					s->TotalPacketSent += 1;
				});
				uWS::WebSocket<uWS::SERVER>::finalizeMessage(preparedMessage);
			}


		};

	}
}




SL::RAT::ServerHub::ServerHub(IServerDriver * r, std::shared_ptr<Server_Config> config)
{
	ServerHubImpl_ = std::make_unique<ServerHubImpl>(r, config);
}

SL::RAT::ServerHub::~ServerHub()
{
}

void SL::RAT::ServerHub::Run()
{
	ServerHubImpl_->h.run();
}

void SL::RAT::ServerHub::Broadcast(char * data, size_t len)
{
	ServerHubImpl_->Broadcast(data, len);
}

int SL::RAT::ServerHub::get_ClientCount() const
{
	return ServerHubImpl_->ClientCount;
}
