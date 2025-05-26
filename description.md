Thanks for the clarification. Here's a **full, detailed description** of the project as outlined in your document:

---

## 📁 Project Title: Distributed File System Over LAN (DFS over LAN)

### 🎯 Project Objectives

1. Understand the architecture and operation of **Distributed File Systems (DFS)**.
2. Learn about the role of DFS in **network environments**, especially over LAN.
3. Familiarize yourself with **Firewall Punching** to enable P2P communication through network firewalls.

---

## 🛠 Tools and Technologies

* Programming Language: **C++**
* Framework: **Qt** (for UI and network functionalities)
* Other Libraries:

  * `schifra` for Reed-Solomon encoding/decoding
  * Potential use of existing UDP socket libraries
* Development focus: Clean, structured, optimized code using modern C++ practices

---

## 📌 Introduction

The project is inspired by the adage: **“Don’t put all your eggs in one basket.”** In large systems, storing all data centrally introduces risks of:

* Failure and data loss
* Performance bottlenecks

Companies like **Google (GFS)** and **Facebook (HDFS)** address these by adopting **Distributed File Systems** where:

* Data is partitioned into **chunks**
* Chunks are stored **across multiple nodes**
* Redundancy and proximity improve **availability** and **speed**

This system mimics those principles over a LAN environment.

---

## 🔧 Architecture of the DFS System

The system consists of two main types of nodes:

### 1. **Manager Node (Metadata Server)**

* Handles metadata such as:

  * File size
  * File name
  * Number of chunks
  * Chunk-to-server mapping
* Decides:

  * How many chunks to split the file into
  * Which chunk server should store which chunk
* Responds to both **write** and **read** requests

### 2. **Chunk Servers**

* Each server stores a specific chunk of data
* Files are split across servers (each server hosts different file parts)
* Chunks are **sequentially linked** (each chunk knows the next chunk's location)

---

## 📤 File Storage Process

1. **User initiates storage** by sending metadata (file size, type, etc.) to the manager.
2. **Manager calculates** how many chunks are needed and selects appropriate chunk servers.
3. **Manager returns**:

   * Server address for first chunk
   * Chunk allocation plan
4. **User splits** the file and sends data to the first chunk server.
5. **Each chunk server**:

   * Stores its chunk
   * Forwards the next chunk’s destination to the client
6. **Repeat** until all chunks are stored.
7. **Manager logs**:

   * File metadata
   * First chunk server address

---

## 📥 File Retrieval Process

1. **User requests** a file by filename from the manager.
2. **Manager returns** the first chunk server address.
3. **User contacts chunk servers** in sequence:

   * Retrieves chunk
   * Follows the link to the next chunk
   * Continues until file is reassembled

---

## 🌲 Network Topology and Algorithm

* Topology: **Binary Tree**
* Each chunk server corresponds to a node.
* **DFS (Depth-First Search)** algorithm is used for traversal and chunk mapping.
* Node ports and folders:

  * Example: `D:/CHUNK-1`, `D:/CHUNK-2` for chunk directories
* Client, Manager, and Chunk Servers must be distributed across **two laptops** on the **same LAN**.

---

## 📎 Noise Injection and Data Corruption Simulation

### Motivation

In real-world systems:

* Data corruption due to **transmission errors**, **storage failure**, or **bit flips** is common.

### Implementation Steps

1. **Inject Noise**:

   * Use a random generator (e.g., `rand()`) to flip bits based on a probability `p`
   * Use `XOR` to flip bits
2. **Ensure Damage**:

   * Corrupt data must not be fully irrecoverable
   * At least one chunk must be corrupted during simulation
3. **Encode Data**:

   * Use **Reed-Solomon Algorithm** to add redundancy
   * Allows recovery from `t/2` bit errors
4. **Decode and Repair**:

   * Use a decoder to attempt reconstruction
   * Track if error correction succeeded
   * Mark irrecoverable chunks as "corrupt"
5. **Libraries**:

   * `schifra` Reed-Solomon encoder/decoder recommended
   * Parameters like `fec_length` define redundancy level

---

## 🔐 Firewall Punching (P2P over UDP)

* Firewalls block unsolicited packets, disrupting direct communication.
* Solution: **UDP Hole Punching** technique

### Scenario

* Two laptops behind firewalls attempt to communicate
* P2P connection is established only **after each sends an outbound packet**
* Requires both sides to open a port by sending a packet first

### Resources

* [GFG article on NAT hole punching](https://www.geeksforgeeks.org/nat-hole-punching-in-computer-network/)
* [Medium article on UDP hole punching](https://medium.com/@girish1729/what-is-udp-hole-punching-9ea109bd5d39)

---

## ❓ Theoretical Questions (To Be Answered in Report)

1. How is noise handled and corrected during file transfer?
2. Why is UDP used in platforms like video streaming despite its unreliability?
3. What happens to data corrupted at rest and how is it detected/fixed?
4. What % of data becomes irrecoverable after applying your noise settings?
5. Are replicas (copies) used to ensure fault-tolerance in DFS?
6. How can DFS help mitigate **network bottlenecks**?
7. Compare **Content Delivery Networks (CDNs)** and DFS — how each improves content availability and access latency.

---

## 📑 Final Requirements

### Functional Criteria

* ✅ File send/receive must work as expected
* ✅ Noise must be added and removed using your algorithm
* ✅ Firewall Punching should function when both devices have firewalls enabled
* ✅ File operations must run across two actual devices on a LAN
* ✅ File/folder naming conventions must be clean and logical
* ✅ Answers to theoretical questions must be well-explained

### Submission Guidelines

* Project must be done in groups of two
* Submit:

  * GitHub repo link (private)
  * Include: all code, a complete `README.md`, and final PDF report
  * Mention: final commit hash in a `.txt` file
* Code structure and documentation are graded
* No ZIP file submission is allowed
* AI-generated solutions without understanding are penalized

---

Let me know if you want this expanded into a **professional report**, **translated into Persian**, or turned into GitHub `README.md` format.
