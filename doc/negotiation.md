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
// Server Cookie format:
//
// 16 bytes: compressed nonce, prefix with "minute-k"
// 80 bytes: secretbox under minute-key, containing:
//     32 bytes: initiator short-term public key
//     32 bytes: responder short-term secret key
//=================================================================================================
// HELLO packet format:
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
// COOKIE packet format:
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
// INITIATE packet format:
//
// 0   : 8     : magic
// 8   : 32    : initiator short-term public key
// 40  : 96    : responder's cookie
//                0  : 16 : compressed nonce
//                16 : 80 : minute-key secretbox containing:
//                           0  : 32 : initiator short-term public key
//                           32 : 32 : responder short-term secret key
// 136 : 8     : compressed nonce
// 144 : 112+M : box C'->S' containing:
// 144 :          0   : 32  : initiator long-term public key
// 176 :          32  : 16  : compressed nonce
// 192 :          48  : 48  : box C->S containing Vouch subpacket:
//                             0 : 32 : initiator short-term public key
// 240 :          96  : M   : message
//
// TOTAL: 240+M+16 bytes
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
// RESPONDER MESSAGE packet format:
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
// INITIATOR MESSAGE packet format:
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
// MESSAGE internal format
// This is a stream/channel message which comprises a continuous stream of bytes.
//
// Integers are in network (big-endian) order. All numbers are unsigned.
//
// 0  : 4 : a message ID chosen by the sender.
// 4  : 4 : if nonzero, a message ID received by the sender immediately before this message was sent.
// 8  : 8 : the number of bytes in the first range being acknowledged as part of this message.
//          A range can include 0 bytes, in which case it does not actually acknowledge anything.
// 16 : 4 : the number of bytes between the first range and the second range.
// 20 : 2 : the number of bytes in the second range.
// 22 : 2 : the number of bytes between the second range and the third range.
// 24 : 2 : the number of bytes in the third range.
// 26 : 2 : the number of bytes between the third range and the fourth range.
// 28 : 2 : the number of bytes in the fourth range.
// 30 : 2 : the number of bytes between the fourth range and the fifth range.
// 32 : 2 : the number of bytes in the fifth range.
// 34 : 2 : the number of bytes between the fifth range and the sixth range.
// 36 : 2 : the number of bytes in the sixth range.
// 38 : 2 : the sum of the following integers:
//          D, an integer between 0 and 1024, the size of the data block being sent
//          as part of this message.
//          SUCC, either 0 or 2048, where 2048 means that this block is known to be
//          at the end of the stream followed by success.
//          FAIL, either 0 or 4096, where 4096 means that this block is known to be
//          at the end of the stream followed by failure.
// 40 : 8 : the position of the first byte in the data block being sent.
//          If D=0 but SUCC>0 or FAIL>0 then this is the success/failure position,
//          i.e., the total number of bytes in the stream.
// 48 : * : Zero-padding. This padding produces a total message length that
//          is a multiple of 16 bytes, at least 16 bytes and at most 1088 bytes.
// *  : D : the data block being sent.
//
// A byte stream is a string of bytes, between 0 bytes and 2^60-1 bytes (allowing more than
// 200 gigabits per second continuously for a year), followed by either success or failure.
// The bytes in an N-byte stream have positions 0,1,2,...,N-1; the success/failure bit has
// position N. A message from the sender can include a block of bytes of data from anywhere
// in the stream; a block can include as many as 1024 bytes. A message from the receiver can
// acknowledge one or more ranges of data that have been successfully received.
//
// The first range acknowledged in a message always begins with position 0. Subsequent ranges
// acknowledged in the same message are limited to 65535 bytes. Each range boundary sent by the
// receiver matches a boundary of a block previously sent by the sender, but a range normally
// includes many blocks.
//
// Once the receiver has acknowledged a range of bytes, the receiver is taking responsibility
// for all of those bytes; the sender is expected to discard those bytes and never send them again.
// The sender can send the bytes again; usually this occurs because the first acknowledgment
// was lost. The receiver discards the redundant bytes and generates a new acknowledgment covering
// those bytes.
//=================================================================================================
