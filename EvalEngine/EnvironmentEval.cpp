// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EnvironmentEval.hpp"
#include "EvalTracer.hpp"
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Pothos/System/Logger.hpp>
#include <Pothos/Util/Network.hpp>
#include <algorithm>
#include <cctype>
#include <optional>

/*!
 * The host of the syslog address for a peering address.
 * The syslog server binds only to IPv4: an IPv4-mapped IPv6 address gives
 * its IPv4 form, and the IPv6 loopback gives the IPv4 loopback.
 * Other IPv6 addresses give no host.
 */
static std::optional<std::string> syslogHostForPeer(std::string addr)
{
    if (addr.size() >= 2 and addr.front() == '[' and addr.back() == ']') addr = addr.substr(1, addr.size()-2);
    if (addr.find(':') == std::string::npos) return addr; //IPv4 (or a host name)

    std::string lower(addr);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){return char(std::tolower(c));});

    //convert IPv6 mapped ports to IPv4 format when possible
    static const std::string mappedPrefix("::ffff:");
    if (lower.starts_with(mappedPrefix))
    {
        const auto v4 = addr.substr(mappedPrefix.size());
        if (v4.find(':') == std::string::npos and v4.find('.') != std::string::npos) return v4;
    }

    //convert an IPv6 loopback address into an IPv4 loopback address
    if (lower == "::1") return std::string("127.0.0.1");

    return std::nullopt;
}

EnvironmentEval::EnvironmentEval(void):
    _failureState(false)
{
    return;
}

EnvironmentEval::~EnvironmentEval(void)
{
    return;
}

void EnvironmentEval::acceptConfig(const QString &zoneName, const QJsonObject &config)
{
    _zoneName = zoneName;
    _config = config;
}

void EnvironmentEval::update(void)
{
    EVAL_TRACER_FUNC_ARG(_zoneName);
    if (this->isFailureState()) _env.reset();

    try
    {
        //env already exists, try test communication
        if (_env)
        {
            _env->findProxy("Pothos/Util/EvalEnvironment");
        }
        //otherwise, make a new env
        else
        {
            _env = this->makeEnvironment();
            auto EvalEnvironment = _env->findProxy("Pothos/Util/EvalEnvironment");
            _eval = EvalEnvironment.call("make");
            _failureState = false;
        }
    }
    catch (const Pothos::Exception &ex)
    {
        //dont report errors if we were already in failure mode
        if (_failureState) return;
        _failureState = true;

        //determine if the remote host is offline or the process just crashed
        const auto hostUri = getHostProcFromConfig(_zoneName, _config).first;
        try
        {
            Pothos::RemoteClient client(hostUri.toStdString());
            _errorMsg = tr("Remote environment %1 crashed").arg(_zoneName);
        }
        catch(const Pothos::RemoteClientError &)
        {
            _errorMsg = tr("Remote host %1 is offline").arg(hostUri);
        }
        POTHOS_LOG_ERROR("PothosFlow.EnvironmentEval", "zone[{}]: {} - {}", _zoneName.toStdString(), ex.displayText(), _errorMsg.toStdString());
    }
}

HostProcPair EnvironmentEval::getHostProcFromConfig(const QString &zoneName, const QJsonObject &config)
{
    if (zoneName == "gui") return HostProcPair(QString::fromStdString("gui://"+Pothos::Util::getLoopbackAddr()), "gui");

    const auto uriDefault = QString::fromStdString("tcp://"+Pothos::Util::getLoopbackAddr());
    const auto hostUri = config["hostUri"].toString(uriDefault);
    const auto processName = config["processName"].toString("");
    return HostProcPair(hostUri, processName);
}

Pothos::ProxyEnvironment::Sptr EnvironmentEval::makeEnvironment(void)
{
    if (_zoneName == "gui") return Pothos::ProxyEnvironment::make("managed");

    const auto hostUri = getHostProcFromConfig(_zoneName, _config).first.toStdString();

    //connect to the remote host and spawn a server
    auto serverEnv = Pothos::RemoteClient(hostUri).makeEnvironment("managed");
    auto serverHandle = serverEnv->findProxy("Pothos/RemoteServer")("tcp://"+Pothos::Util::getWildcardAddr(), false/*noclose*/);

    //construct the uri for the new server
    std::string actualPort = serverHandle.call("getActualPort");
    auto newHostUri = Pothos::Util::parseUri(hostUri);
    newHostUri.port = static_cast<std::uint16_t>(std::stoul(actualPort));

    //connect the client environment
    auto client = Pothos::RemoteClient(Pothos::Util::toString(newHostUri));
    client.holdRef(Pothos::Object(serverHandle));
    auto env = client.makeEnvironment("managed");

    //determine log delivery address
    //FIXME syslog listener doesn't support IPv6, special precautions taken:
    const auto logSource = (not _zoneName.isEmpty())? _zoneName.toStdString() : newHostUri.host;
    const auto syslogListenPort = Pothos::System::Logger::startSyslogListener();
    const auto syslogHost = syslogHostForPeer(env->getPeeringAddress());
    if (not syslogHost)
    {
        //otherwise warn because the forwarding will not work
        POTHOS_LOG_WARNING("PothosFlow.EnvironmentEval", "Log forwarding not supported over IPv6: {}", logSource);
        return env;
    }
    const auto serverAddr = *syslogHost + ":" + syslogListenPort;

    //setup log delivery from the server process
    env->findProxy("Pothos/System/Logger").call("startSyslogForwarding", serverAddr);
    env->findProxy("Pothos/System/Logger").call("forwardStdIoToLogging", logSource);
    serverHandle.call("startSyslogForwarding", serverAddr, logSource);

    return env;
}
