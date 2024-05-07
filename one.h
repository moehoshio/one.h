#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>

namespace one {

    using namespace std::literals;
    using exception_ = std::runtime_error; // your exception type
    
    inline namespace Opt {
        // Indicates the use of the default constructor.
        struct notUseConditionConstructor{};
    }; // namespace Opt

    // base, save list
    template <typename T, typename... Args>
    class oneMethod {
    private:
        static std::vector<std::tuple<Args...>> list;
    protected:
        static std::timed_mutex mtx;

    public:
        template <typename... Argss>
        static void add(Argss &&...args) noexcept {
            list.push_back(std::make_tuple(std::forward<Argss...>(args...)));
        }

        static void add(std::tuple<Args...> &&t) noexcept {
            list.push_back(t);
        }
        template <typename... Argss>
        static bool verified(Argss &&...args) noexcept {
            return (std::find(list.begin(), list.end(), std::make_tuple(std::forward<Argss...>(args...))) != list.end());
        }

        static bool verified(std::tuple<Args...> &&t) noexcept {
            return (std::find(list.begin(), list.end(), t) != list.end());
        }
        template <typename... Argss>
        static void erase(Argss &&...args) noexcept {
            list.erase(std::remove(list.begin(), list.end(), std::make_tuple(std::forward<Argss...>(args...))), list.end());
        }

        static void erase(std::tuple<Args...> &&t) noexcept {
            list.erase(std::remove(list.begin(), list.end(), t), list.end());
        }
    };
    template <typename T, typename... Args>
    std::vector<std::tuple<Args...>> oneMethod<T, Args...>::list;
    template <typename T, typename... Args>
    std::timed_mutex oneMethod<T, Args...>::mtx;

    /// @brief Construct a T type object using parameters and ensure that there is only one instance of T object with the same parameters
    // need args identical , e.g <std::string,std::string> and <std::string,std::string> ,Only then will this be effective (Otherwise it is a different instance)
    // if is Custom type, there should be T&& Construct 、operator == and operator &&== 、new not =delete .
    // Here is an example that meets the criteria:
    /*
    struct X{
        int i = 0;
        X(const X& x): i(x){}
        bool operator==(const X& x){return (i==x)? true : false ;}
        X& operator=(const X& x) { i = x.i; return *this;}
    };
    */
    /// @param Args... , If it's a pointer, the comparison will be based on the pointer address (including char*!), To compare whether strings are the same, std::string should be used
    /// @param condition Condition generates one additional copy.
    template <typename T, typename... Args>
    class one : private oneMethod<T, Args...> {
        
    private:
        std::tuple<Args...> data;
        struct Base {
            virtual inline T *get() = 0;
            virtual ~Base() {}
        };
        Base *base;
        struct retain : public Base {
            T *ptr;
            retain() {
                ptr = new T();
            };
            template <typename... Argss>
            retain(Argss &&...args) {
                ptr = new T{args...};
            }
            inline T *get() {
                return ptr;
            }
            ~retain() {
                delete ptr;
            }
        };
        struct package : public Base {
            T &obj;
            package(T &o) : obj(o){};
            inline T *get() {
                return &obj;
            };
        };

        template <typename... Argss>
        inline void entrust(const std::chrono::milliseconds &t, Argss &&...condition) {

            // if need Guaranteed not to be modified during traversal ,Just get the lock first(Need Unlock before throwing)
            if (this->verified(std::forward<Argss...>(condition...)))
                throw exception_("There is the same");

            if (!this->mtx.try_lock_for(t))
                throw exception_("Get lock the time out");

            this->add(std::forward<Argss...>(condition...));
            data = std::make_tuple(std::forward<Argss...>(condition...));
            this->mtx.unlock();
        }

    public:
        // The default constructor does nothing; if used, the init function should be called.
        one() noexcept {}
        // Constructing objects using constructArgs.
        template <typename... Argss>
        one(Args... condition, std::chrono::milliseconds t, Argss &&...constructArgs) {
            entrust(t, std::move(condition...));
            base = new retain(std::forward<Argss...>(constructArgs...));
        }
        // Constructing objects using constructArgs.
        template <typename... Argss>
        one(Args... condition, Argss &&...constructArgs) : one(condition..., 5000min , constructArgs...){};

        // Constructing objects using condition.
        one(Args... condition, std::chrono::milliseconds t = 5000min) {
            entrust(t, std::move(condition...));
            base = new retain(condition...);
        }
        /// @param o Indicates the use of the default constructor.
        one(const Opt::notUseConditionConstructor &o, Args... condition, std::chrono::milliseconds t = 5000min) {
            entrust(t, std::move(condition...));
            base = new retain();
        }
        // Passing reference wrappers for use after construction by the user.
        // For move semantics: right-value references should be passed directly as construction parameters; please select another constructor version.
        //  If the same condition already exists , or time out get lock ,throw ex.
        one(T &d, Args... condition, std::chrono::milliseconds t = 5000min) {
            entrust(t, std::move(condition...));
            base = new package(d);
        }

        // If the same condition already exists , or time out get lock ,ret false.
        // If the returns false, it can be called again until successful. There should be no resource leaks.
        template <typename... Argss>
        inline bool init(Args... condition, std::chrono::milliseconds t, Argss &&...constructArgs) noexcept {
            try {
                entrust(t, std::move(condition...));
                base = new retain(constructArgs...);
                return true;
            } catch (...) {
                return false;
            }
        }
        // If the same condition already exists , or time out get lock ,ret false.
        // If the returns false, it can be called again until successful. There should be no resource leaks.
        template <typename... Argss>
        inline bool init(Args... condition, Argss &&...constructArgs)noexcept {
            return init(condition..., 5000min, constructArgs...);
        }
        // Passing reference wrappers for use after construction by the user.
        // For move semantics: right-value references should be passed directly as construction parameters; please select another constructor version.
        //  If the same condition already exists , or time out get lock ,throw ex.
        // If the returns false, it can be called again until successful. There should be no resource leaks.
        inline bool init(T &d, Args... condition, std::chrono::milliseconds t = 5000min) noexcept {
            try {
                entrust(t, std::move(condition...));
                base = new package(d);
                return true;
            } catch (...) {
                return false;
            }
        }
        /// @param o Indicates the use of the default constructor.
        //// If the same condition already exists , or time out get lock ,ret false.
        // If the returns false, it can be called again until successful. There should be no resource leaks.
        inline bool init(const Opt::notUseConditionConstructor &o, Args... condition, std::chrono::milliseconds t = 5000min ) noexcept {
            try {
                entrust(t, std::move(condition...));
                base = new retain();
                return true;
            } catch (...) {
                return false;
            }
        }

        ~one() noexcept {
            this->mtx.lock();
            this->erase(std::move(data));
            delete base;
            this->mtx.unlock();
        }

        inline operator T &() noexcept {
            return *base->get();
        }

        inline operator T *() const noexcept {
            return base->get();
        }

        inline T *get() const noexcept {
            return base->get();
        }
    };

    // It differs from `one` in that it can directly access the member object, directly holding the member instead of a pointer.
    /// @brief Construct a T type object using parameters and ensure that there is only one instance of T object with the same parameters
    // need args identical , e.g <std::string,std::string> and <std::string,std::string> ,Only then will this be effective (Otherwise it is a different instance)
    // if is Custom type, there should be T&& Construct 、operator == and operator &&== 、new not =delete
    /// @param Args... , If it's a pointer, the comparison will be based on the pointer address (including char*!), To compare whether strings are the same, std::string should be used
    /// @param condition Condition generates one additional copy.
    template <typename T, typename... Args>
    struct oneR : private oneMethod<T, Args...> {
        T obj;
        std::tuple<Args...> data;
        template <typename... Argss>
        inline void entrust(std::chrono::milliseconds t, Argss &&...args) {
            // if need Guaranteed not to be modified during traversal ,Just get the lock first(Need Unlock before throwing)
            if (this->verified(std::forward<Argss...>(args...)))
                throw exception_("There is the same");

            if (!this->mtx.try_lock_for(t))
                throw exception_("Get lock the time out");

            this->add(std::forward<Argss...>(args...));
            data = std::make_tuple(std::forward<Argss...>(args...));
            this->mtx.unlock();
        }
        oneR() noexcept {};
        template <typename... Argss>
        oneR(std::chrono::milliseconds t, Args... condition, Argss &&...args) {
            entrust(t, std::move(condition...));
            obj = T{std::forward<Argss...>(args...)};
        };
        oneR(const Opt::notUseConditionConstructor &o, Args... condition, std::chrono::milliseconds t = std::chrono::milliseconds(5000U)) {
            entrust(t, std::move(condition...));
        }
        bool init(const Opt::notUseConditionConstructor &o, Args... condition, std::chrono::milliseconds t = std::chrono::milliseconds(5000U)) {
            try {
                entrust(t, std::move(condition...));
                return true;
            } catch (...) {
                return false;
            }
        }
        template <typename... Argss>
        bool init(std::chrono::milliseconds t, Args... condition, Argss &&...args) noexcept {
            try {
                entrust(t, std::move(condition...));
                obj = T{std::forward<Argss...>(args...)};
                return true;
            } catch (...) {
                return false;
            }
        }
        ~oneR() {
            this->mtx.lock();
            this->erase(data);
            this->mtx.unlock();
        }
    };

} // namespace one
