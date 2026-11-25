#include "ExternalRobotTelemetry.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <OleAuto.h>

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace robot_telemetry
{
    namespace {
        std::wstring wide(const std::string& value) {
            const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0);
            std::wstring result(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.data(), int(value.size()), result.data(), size);
            return result;
        }
        std::string utf8(const wchar_t* value) {
            if(!value) return {};
            const int count = int(wcslen(value));
            const int size = WideCharToMultiByte(CP_UTF8, 0, value, count, nullptr, 0, nullptr, nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, value, count, result.data(), size, nullptr, nullptr);
            return result;
        }
        void check(HRESULT hr, const char* action) {
            if(FAILED(hr)) {
                std::ostringstream text; text << action << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
                throw std::runtime_error(text.str());
            }
        }
        struct Apartment {
            Apartment() { check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "Initialize COM"); }
            ~Apartment() { CoUninitialize(); }
        };
        struct Value {
            VARIANT data;
            Value() { VariantInit(&data); }
            explicit Value(double* value) : Value() { data.vt = VT_R8 | VT_BYREF; data.pdblVal = value; }
            explicit Value(int value) : Value() { data.vt = VT_I2; data.iVal = SHORT(value); }
            explicit Value(const std::string& value) : Value() { data.vt = VT_BSTR; const auto w = wide(value); data.bstrVal = SysAllocStringLen(w.data(), UINT(w.size())); }
            Value(const Value& other) : Value() { check(VariantCopy(&data, const_cast<VARIANT*>(&other.data)), "Copy COM value"); }
            Value(Value&& other) noexcept : data(other.data) { VariantInit(&other.data); }
            ~Value() { VariantClear(&data); }
            double number() const {
                Value v; check(VariantChangeType(&v.data, const_cast<VARIANT*>(&data), 0, VT_R8), "Convert COM number"); return v.data.dblVal;
            }
            std::string string() const {
                Value v; check(VariantChangeType(&v.data, const_cast<VARIANT*>(&data), 0, VT_BSTR), "Convert COM string"); return utf8(v.data.bstrVal);
            }
        };
        class Dispatch {
        public:
            IDispatch* p = nullptr;
            Dispatch() = default;
            explicit Dispatch(const Value& v) {
                if(v.data.vt == VT_DISPATCH && v.data.pdispVal) { p = v.data.pdispVal; p->AddRef(); }
                else throw std::runtime_error("Missing FANUC COM dispatch object");
            }
            Dispatch(const Dispatch& other) : p(other.p) { if(p) p->AddRef(); }
            Dispatch(Dispatch&& other) noexcept : p(other.p) { other.p = nullptr; }
            Dispatch& operator=(Dispatch other) noexcept { std::swap(p, other.p); return *this; }
            ~Dispatch() { if(p) p->Release(); }
            Value get(const wchar_t* name, std::initializer_list<Value> args = {}, WORD flags = DISPATCH_PROPERTYGET | DISPATCH_METHOD) const {
                if(!p) throw std::runtime_error("FANUC COM object unavailable");
                DISPID id; LPOLESTR member = const_cast<LPOLESTR>(name);
                check(p->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_USER_DEFAULT, &id), utf8(name).c_str());
                std::vector<VARIANT> reversed;
                for(auto i = args.end(); i != args.begin();) { --i; reversed.push_back(i->data); }
                DISPPARAMS params{}; params.rgvarg = reversed.data(); params.cArgs = UINT(reversed.size());
                Value result; EXCEPINFO exception{}; UINT bad = 0;
                const auto hr = p->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, &result.data, &exception, &bad);
                const auto detail = utf8(exception.bstrDescription);
                SysFreeString(exception.bstrSource); SysFreeString(exception.bstrDescription); SysFreeString(exception.bstrHelpFile);
                if(FAILED(hr) && !detail.empty()) throw std::runtime_error(utf8(name) + ": " + detail);
                check(hr, utf8(name).c_str());
                return result;
            }
        };
        class RoboguideBackend final : public Backend {
            Apartment apartment; // Must outlive all dispatch objects.
            Dispatch neighborhood;
            std::map<std::string, Dispatch> robots;
            Dispatch robot;
            Dispatch server;
            Dispatch position;
            Dispatch group;
            int motionGroup = 1;
            bool worldDisplay = false;
            std::string poseUnavailable;
            int visited = 0;
            void traverse(const Dispatch& folder, int depth, std::vector<Endpoint>& found) {
                if(depth > 32 || ++visited > 10000) throw std::runtime_error("Robot Neighborhood tree exceeds discovery limit");
                const int count = int(folder.get(L"Count").number());
                if(count < 0 || count > 10000) throw std::runtime_error("Invalid Robot Neighborhood count");
                for(int i = 0; i < count; ++i) {
                    Dispatch item(folder.get(L"Item", {Value(std::string()), Value(i)}));
                    bool isFolder = false;
                    try { item.get(L"Count"); isFolder = true; } catch(const std::exception&) {}
                    if(isFolder) { traverse(item, depth + 1, found); continue; }
                    if(int(item.get(L"Type").number()) != 1 || int(item.get(L"ConnectionStatus").number()) != 3) continue;
                    const auto id = item.get(L"PathName").string();
                    found.push_back({id, item.get(L"Name").string()});
                    robots.emplace(id, std::move(item));
                }
            }
        public:
            RoboguideBackend() {
                CLSID clsid;
                check(CLSIDFromProgID(L"FRRobotNeighborhood.FRCRobotNeighborhood", &clsid), "FANUC Robot Neighborhood is not installed/registered");
                check(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_IDispatch,
                    reinterpret_cast<void**>(&neighborhood.p)), "Open FANUC Robot Neighborhood");
            }
            std::vector<Endpoint> discover() override {
                robots.clear(); visited = 0;
                std::vector<Endpoint> result;
                traverse(Dispatch(neighborhood.get(L"Robots")), 0, result);
                return result;
            }
            void attach(const std::string& endpoint, int number) override {
                const auto found = robots.find(endpoint);
                if(found == robots.end()) throw std::runtime_error("Selected virtual controller is no longer available");
                robot = found->second;
                if(int(robot.get(L"ConnectionStatus").number()) != 3) throw std::runtime_error("Selected virtual controller disconnected");
                server = Dispatch(robot.get(L"RobotServer"));
                if(server.get(L"IsConnected").number() == 0) throw std::runtime_error("Robot Server disconnected");
                position = Dispatch(server.get(L"CurPosition"));
                motionGroup = number;
                if(number < 1 || number > int(position.get(L"NumGroups").number())) throw std::runtime_error("Selected motion group does not exist");
                try {
                    group = Dispatch(position.get(L"Group", {Value(number), Value(2)}));
                    worldDisplay = true;
                } catch(const std::exception& error) {
                    // Some non-Cartesian motion groups expose joints only.
                    poseUnavailable = error.what();
                    group = Dispatch(position.get(L"Group", {Value(number), Value(0)}));
                }
            }
            Reading read() override {
                MSG message;
                while(PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
                if(int(robot.get(L"ConnectionStatus").number()) != 3 || server.get(L"IsConnected").number() == 0)
                    throw std::runtime_error("ROBOGUIDE controller connection lost");
                // Formats(frJoint) changes the cached position representation. Request
                // WORLD again before each refresh, and capture Cartesian data first.
                bool cartesianAvailable = worldDisplay;
                std::string unavailable = poseUnavailable;
                try {
                    if(worldDisplay) group = Dispatch(position.get(L"Group", {Value(motionGroup), Value(2)}));
                    group.get(L"Refresh", {}, DISPATCH_METHOD);
                } catch(const std::exception& error) {
                    if(!worldDisplay) throw;
                    unavailable = error.what();
                    cartesianAvailable = false;
                    group = Dispatch(position.get(L"Group", {Value(motionGroup), Value(0)}));
                    group.get(L"Refresh", {}, DISPATCH_METHOD);
                }
                Reading result;
                if(cartesianAvailable) {
                    try {
                        CartesianPose pose;
                        pose.userFrame = int(group.get(L"UserFrame").number());
                        pose.userTool = int(group.get(L"UserTool").number());
                        if(pose.userFrame != 0 || pose.userTool < 0)
                            throw std::runtime_error("WORLD TCP identity unavailable: UF=" + std::to_string(pose.userFrame)
                                + " UT=" + std::to_string(pose.userTool));
                        Dispatch cartesian(group.get(L"Formats", {Value(2)}));
                        try {
                            cartesian.get(L"GetAll", {Value(&pose.xyzwpr[0]), Value(&pose.xyzwpr[1]), Value(&pose.xyzwpr[2]),
                                Value(&pose.xyzwpr[3]), Value(&pose.xyzwpr[4]), Value(&pose.xyzwpr[5])}, DISPATCH_METHOD);
                        } catch(const std::exception&) {
                            const wchar_t* fields[] = {L"X", L"Y", L"Z", L"W", L"P", L"R"};
                            for(int i = 0; i < 6; ++i) pose.xyzwpr[i] = cartesian.get(fields[i]).number();
                        }
                        pose.valid = true;
                        result.tcpPose = std::move(pose);
                    } catch(const std::exception& error) {
                        result.tcpPose.diagnostic = error.what();
                    }
                } else result.tcpPose.diagnostic = "WORLD Cartesian pose unavailable: " + unavailable;
                Dispatch joints(group.get(L"Formats", {Value(9)}));
                const int count = int(joints.get(L"Count").number());
                if(count < 1 || count > 64) throw std::runtime_error("Unsupported controller joint count");
                for(int i = 1; i <= count; ++i) result.joints.push_back(joints.get(L"Item", {Value(i)}).number());
                return result;
            }
        };
    }
    std::unique_ptr<Backend> makeRoboguideBackend() { return std::make_unique<RoboguideBackend>(); }
}
#else
#include <stdexcept>
namespace robot_telemetry {
    std::unique_ptr<Backend> makeRoboguideBackend() { throw std::runtime_error("ROBOGUIDE COM requires Windows"); }
}
#endif
