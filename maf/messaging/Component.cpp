#include <maf/messaging/MessageQueue.h>
#include <maf/messaging/Component.h>
#include <maf/utils/cppextension/SyncObject.h>
#include <maf/threading/TimerManager.h>
#include <maf/messaging/BasicMessages.h>
#include <maf/utils/debugging/Debug.h>
#include <thread>
#include <memory>
#include <map>


namespace maf {
namespace messaging {

using MsgHandlerMap = nstl::SyncObject<std::map<MessageBase::Type, MessageHandlerFunc<MessageBase>>>;
using TimerMgrPtr = std::shared_ptr<threading::TimerManager>;

static thread_local ComponentRef _tlwpInstance;

struct Component::ComponentImpl
{
public:
    ComponentImpl();
    ~ComponentImpl();
    void run(ComponentRef compref, LaunchMode LaunchMode, std::function<void()> onEntry, std::function<void()> onExit);
    void stop();
    void postMessage(MessageBasePtr msg);
    void registerMessageHandler(MessageBase::Type msgType, MessageHandler* handler);
    void registerMessageHandler(MessageBase::Type msgType, BaseMessageHandlerFunc onMessageFunc);
    TimerMgrPtr getTimerManager();
    void startMessageLoop(ComponentRef compref, std::function<void()> onEntry, std::function<void()> onExit);
private:
    ComponentRef _compref;
    std::thread _workerThread;
    MessageQueue _msgQueue;
    MsgHandlerMap _msgHandlers;
    TimerMgrPtr _timerMgr;
};


Component::ComponentImpl::ComponentImpl()
{
    registerMessageHandler(MessageBase::idof<TimeoutMessage>(), [](const auto& msg) {
        auto timeoutMsg = std::static_pointer_cast<TimeoutMessage>(msg);
        timeoutMsg->execute();
    });
    registerMessageHandler(MessageBase::idof<CallbackExcMsg>(), [](const auto& msg) {
        auto cbExcMsg = std::static_pointer_cast<CallbackExcMsg>(msg);
        cbExcMsg->execute();
    });
}

Component::ComponentImpl::~ComponentImpl()
{
    stop();
}

void Component::ComponentImpl::run(ComponentRef compref, LaunchMode LaunchMode, std::function<void()> onEntry, std::function<void()> onExit)
{
    if(LaunchMode == LaunchMode::Async)
    {
        _workerThread = std::thread {
            [this, compref, onEntry, onExit] {
            this->startMessageLoop(compref, onEntry, onExit);
        }};
    }
    else
    {
        this->startMessageLoop(compref, onEntry, onExit);
    }
}

void Component::ComponentImpl::stop()
{
    _msgQueue.close();
	if (_timerMgr) { _timerMgr->stop(); _timerMgr.reset(); }
    if(std::this_thread::get_id() != _workerThread.get_id())
    {
        if(_workerThread.joinable())
        {
            _workerThread.join();
        }
    }
}

void Component::ComponentImpl::postMessage(MessageBasePtr msg)
{
    try
    {
        _msgQueue.push(std::move(msg));
    }
    catch(const std::bad_alloc& ba)
    {
        mafErr("Queue overflow: " << ba.what());
    }
    catch(const std::exception& e)
    {
        mafErr("Exception occurred when pushing data to queue: " << e.what());
    }
    catch(...)
    {
        mafErr("Unkown exception occurred when pushing data to queue!");
    }
}

void Component::ComponentImpl::registerMessageHandler(MessageBase::Type msgType, MessageHandler *handler)
{
    if(handler)
    {
        registerMessageHandler(msgType, [handler](const std::shared_ptr<MessageBase>& msg) { handler->onMessage(msg);});
    }
}

void Component::ComponentImpl::registerMessageHandler(MessageBase::Type msgType, BaseMessageHandlerFunc onMessageFunc)
{
    if(onMessageFunc)
    {
        auto lock = _msgHandlers.a_lock();
        _msgHandlers->insert(std::make_pair(msgType, onMessageFunc));
    }
}

TimerMgrPtr Component::ComponentImpl::getTimerManager()
{
    if(!_timerMgr)
    {
        _timerMgr = std::make_shared<threading::TimerManager>();
    }
    return _timerMgr; // at this point, the timer object will never be destroyed if still have someone holding its reference(shared_ptr)
}

void Component::ComponentImpl::startMessageLoop(ComponentRef compref, std::function<void()> onEntry, std::function<void()> onExit)
{
    _compref = _tlwpInstance = compref;
    if(onEntry)
    {
        if(auto component = _compref.lock()) onEntry();
    }

    MessageBasePtr msg;
    while(_msgQueue.wait(msg))
    {
        if(!msg)
        {
            mafErr("Got nullptr message");
            continue;
        }
        else
        {
            BaseMessageHandlerFunc handlerFunc;
            {
                auto lock = _msgHandlers.a_lock();
                auto itHandler = _msgHandlers->find(MessageBase::idof(*msg));
                if(itHandler != _msgHandlers->end())
                {
                    handlerFunc = itHandler->second;
                }
            }

            if(handlerFunc)
            {
                try
                {
                    handlerFunc(msg);
                }
                catch(const std::exception& e)
                {
                    mafErr("Exception occurred while executing messageHandler function: " << e.what());
                }
                catch(...)
                {
                    mafErr("Unknown exception occurred while executing messageHandler function: ");
                }
            }
            else
            {
                mafWarn("There's no handler for message " << MessageBase::idof(*msg).name());
            }
        }
    }

    if(onExit)
    {
        if(auto component = _compref.lock()) onExit();
    }
}

Component::Component() : _pImpl{ new ComponentImpl } {}
Component::~Component() { if(_pImpl) delete _pImpl; }

std::shared_ptr<Component> Component::create() { return std::shared_ptr<Component>{ new Component};}
const std::string &Component::name() const { return _name; }
void Component::setName(std::string name) { _name = std::move(name); }
void Component::run(LaunchMode LaunchMode, std::function<void()> onEntry, std::function<void()> onExit) { _pImpl->run(weak_from_this(), LaunchMode, onEntry, onExit); }
void Component::stop(){ _pImpl->stop();}
void Component::postMessage(MessageBasePtr msg) { _pImpl->postMessage(msg); }
void Component::registerMessageHandler(MessageBase::Type msgType, MessageHandler *handler) { _pImpl->registerMessageHandler(msgType, handler); }
void Component::registerMessageHandler(MessageBase::Type msgType, BaseMessageHandlerFunc onMessageFunc){ _pImpl->registerMessageHandler(msgType, std::move(onMessageFunc)); }
ComponentRef Component::getActiveWeakPtr() { return _tlwpInstance; }
std::shared_ptr<Component> Component::getActiveSharedPtr(){ return _tlwpInstance.lock(); }

TimerMgrPtr Component::getTimerManager()
{
    auto spInstance = _tlwpInstance.lock();
    if(spInstance)
    {
        return spInstance->_pImpl->getTimerManager();
    }
    else
    {
        return {};
    }
}

}
}
