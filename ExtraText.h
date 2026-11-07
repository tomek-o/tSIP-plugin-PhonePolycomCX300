#ifndef ExtraTextH
#define ExtraTextH

#include <string>

/** \brief Optional extra text shown on the phone display (after the clock).

    The text can be set from outside, without any tSIP involvement:
    - UDP datagram sent to 127.0.0.1:<extraTextUdpPort>
    - first line of a text file (<extraTextFile>), re-read when its modification time changes
    The most recent update wins. An empty datagram / empty file clears the text.

    All functions must be called from the same thread (the comm thread).
*/
namespace ExtraText
{
    /** \brief Open UDP socket and prepare the file source, according to customConf */
    void Init(void);

    /** \brief Process pending UDP datagrams, check the file. Call often (non-blocking). */
    void Poll(void);

    void Close(void);

    /** \brief Current, sanitized text, truncated to extraTextMaxChars */
    std::string Get(void);

    /** \brief Returns true (once) if the text changed since the last call */
    bool ConsumeChanged(void);

    /** \brief Replace non-printable / non-ASCII characters, trim spaces and limit length.
        \note exposed for testing
    */
    std::string Sanitize(const std::string &in, unsigned int maxChars);
}

#endif // ExtraTextH
