#ifndef C4_CONF_EXPORT_HPP_
#define C4_CONF_EXPORT_HPP_

#ifdef _WIN32
    #ifdef C4CONF_SHARED
        #ifdef C4CONF_EXPORTS
            #define C4CONF_EXPORT __declspec(dllexport)
        #else
            #define C4CONF_EXPORT __declspec(dllimport)
        #endif
    #else
        #define C4CONF_EXPORT
    #endif
#else
    #define C4CONF_EXPORT
#endif

#endif /* C4_CONF_EXPORT_HPP_ */
