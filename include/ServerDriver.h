#pragma once
#include <memory>
#include <vector>

namespace SL {
	namespace Screen_Capture {
		struct Image;
		struct Monitor;
	};
	namespace RAT {
		class Point;

		class ServerDriverImpl;
		struct Server_Config;
		class IServerDriver;
		class IWebSocket;
		class ServerDriver {
			std::unique_ptr<ServerDriverImpl> ServerDriverImpl_;

		public:
			ServerDriver(IServerDriver * r, std::shared_ptr<Server_Config> config);
			~ServerDriver();

			void Run();
			//frames are images
			void SendFrameChange(IWebSocket* socket, const Screen_Capture::Image & img, const SL::Screen_Capture::Monitor& monitor);
			void SendMonitorInfo(IWebSocket* socket, const std::vector<std::shared_ptr<Screen_Capture::Monitor>> & monitors);

			void SendMouse(IWebSocket* socket, const Screen_Capture::Image & img);
			void SendMouse(IWebSocket* socket, const Point& pos);
			void SendClipboardText(IWebSocket* socket, const char* data, unsigned int len);

		};


	}
}
