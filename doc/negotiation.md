// CurveCP datagrams are specified to always fit in the smallest IPv6 datagram, 1280 bytes.
// (plus a typical 20 byte IPv4 and 8 byte UDP header)

// Attached to the box is a public 24-byte nonce chosen by the sender.
// Nonce means "number used once." After a particular nonce has been used to encrypt a packet,
// the same nonce must never be used to encrypt another packet
// from the sender's secret key to this receiver's public key,
// and the same nonce must never be used to encrypt another packet
// from the receiver's secret key to this sender's public key.
// This requirement is essential for cryptographic security.

//=================================================================================================
// Cookie format:
//
// 16 bytes: compressed nonce, prefix with "minute-k"
// 80 bytes: secretbox under minute-key, containing:
//     32 bytes: initiator short-term public key
//     32 bytes: responder short-term secret key
//=================================================================================================
// HELLO format:
//
// 0   : 8  : magic
// 8   : 32 : initiator short-term public key
// 40  : 64 : zero
// 104 : 8  : compressed nonce
// 112 : 80 : box C'->S containing:
//             0  : 32 : initiator long-term public key (for pre-auth)
//             32 : 32 : zero
//
// TOTAL: 192 bytes
//=================================================================================================
// COOKIE format:
//
// In response to Hello packet, the Responder does not allocate any state. Instead, it encodes information
// about the Initiator into the returned Cookie. If the Initiator is willing to continue session it
// responds with an Initiate packet, which may contain initial message data along with identifying Cookie.
//
// In response, Responder encodes initiator's short-term public key and own short-term secret key using
// a special minute key, which is rotated every minute. If session isn't started within this interval,
// the responder will not be able to open this box and will discard the Initiate packet, thus failing
// session negotiation.
//
// 0  : 8   : magic
// 8  : 16  : compressed nonce
// 24 : 144 : box S->C' containing:
//             0  : 32 : responder short-term public key
//             32 : 16 : compressed nonce
//             48 : 80 : minute-key secretbox containing:
//                        0  : 32 : initiator short-term public key
//                        32 : 32 : responder short-term secret key
//
// TOTAL: 168 bytes
//=================================================================================================
// INITIATE format:
//
// 0   : 8     : magic
// 8   : 32    : initiator short-term public key
// 40  : 96    : responder's cookie
//                0  : 16 : compressed nonce
//                16 : 80 : minute-key secretbox containing:
//                           0  : 32 : initiator short-term public key
//                           32 : 32 : responder short-term secret key
// 136 : 8     : compressed nonce
// 144 : 368+M : box C'->S' containing:
// 144 :          0   : 32  : initiator long-term public key
// 176 :          32  : 16  : compressed nonce
// 192 :          48  : 48  : box C->S containing Vouch subpacket:
//                             0 : 32 : initiator short-term public key
// 240 :          96  : M   : message
//
// TOTAL: 240+M bytes
//
// M size is in multiples of 16 between 16 and 1024 bytes.
//
// When Initiate passes validation in the packet pump, the C'->S' box
// gets replaced with the plaintext contents of the box. This is so
// that the conn handler doesn't need to double-decode retransmitted
// initiates. The absolute numbers for box elements are only valid
// once the packet has been verified and the plaintext content copied
// over the box.
//
// When Initiate packet is accepted, starting a session, cookie must be places into a cache
// and cleaned when minute key is rotated.
//
//=================================================================================================
// RESPONDER MESSAGE format:
//
// 0  : 8    : magic
// 8  : 32   : responder short-term public key S'
// 40 : 8    : compressed nonce
// 48 : 16+M : box S'->C' containing:
//              0 : M : message
//
// M size is in multiples of 16 between 48 and 1088 bytes.
//
// TOTAL: 64+M bytes
//=================================================================================================
// INITIATOR MESSAGE format:
//
// 0   : 8    : magic
// 8   : 32   : initiator short-term public key C'
// 40  : 8    : compressed nonce
// 48  : 16+M : box C'->S' containing:
//               0 : M : message
//
// M size is in multiples of 16 between 48 and 1088 bytes.
//
// TOTAL: 64+M bytes
//=================================================================================================
