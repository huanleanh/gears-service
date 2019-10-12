#include <iostream>
#include <maf/messaging/client-server/ipc/LocalIPCServiceStub.h>
#include <maf/messaging/client-server/ipc/LocalIPCServer.h>
#include <maf/messaging/ExtensibleComponent.h>
#include <maf/messaging/Timer.h>
#include "GearsContract.h"

using namespace maf::messaging;

class ScanServiceComp : public ExtensibleComponent
{
public:
	maf::messaging::Timer _timer;
	int _percentage = 0;
	std::shared_ptr<ipc::LocalIPCServiceStub> _serviceStub;

	void onEntry() override
	{
		onMessage<ipc::IPCClientRequestMsg>([this](CMessagePtr<ipc::IPCClientRequestMsg>& msg) {
			auto requestKeeper = msg->getRequestKeeper();
			if (requestKeeper->getOperationCode() == OpCode::Request && requestKeeper->getOperationID() == ScanningStatus::ID())
			{
				//_timer.start(1000, [requestKeeper, this] {
				auto status = ScanningStatus::makeResult();
				for (int i = 1; i <= 20; i++) {
					std::this_thread::sleep_for(std::chrono::seconds(1));
					status->set_percentage(i);
					requestKeeper->update(status);
				}
				/*auto status = ScanningStatusResult::create();
				std::this_thread::sleep_for(std::chrono::seconds(1));
				status->props().set_percentage(_percentage++);
				requestKeeper->update(status);*/
				//});
				requestKeeper->respond(status);
			}
			/*else if (requestKeeper->getOperationCode() == OpCode::Abort)
			{
				_timer.stop();
			}*/
			});

		_serviceStub = ipc::LocalIPCServiceStub::createStub(SERVICE_ID_SCANNING_SERVICE);
	}
};

int main()
{
	ipc::LocalIPCServer::instance().init(Address{ SERVER_DOMAIN_NAME, 0 });
	ScanServiceComp comp;
	comp.run(maf::messaging::LaunchMode::AttachToCurrentThread);
}