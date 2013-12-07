//
// Run a simulated replay of network traffic between two nodes
// extracted from the dump.bin files (name one local.dump for the local side, remote.dump
// for the remote side).
//

// Read first packet from both dumps,
// Set time baseline based on the smallest timestamp
// 3. Push packet with smallest timestamp to the sim_link with appropriate delivery time
// This side's packet is now empty and available for reading into form the dump
// Run one simulation step
// Read into the available slot from the appropriate dump
// Go to step 3
// Repeat this until out of packets in both dumps

// NB: use "receive raw packet" to insert data only
// TODO: simulator would try to create and insert its own packets into the queue
//       need to actually compare them to the reference ones in the dumps and maybe replace.
//       keep information about mismatches at least...
