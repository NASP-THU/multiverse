#pragma once

#include "../types.hpp"
#include <type_traits>
#include <cstring>
#include <cstdio>

namespace madEscape {
    // random generator (Linear Congruential Generator)
    class LCG {
        private:
            unsigned long a;
            unsigned long c;
            unsigned long modulus;
            unsigned long seed;

        public:
            LCG(unsigned long _a, unsigned long _c, unsigned long _modulus, unsigned long _seed);
            unsigned long next();
            void set_seed(unsigned long _seed);
    };

    //the operation set for queue managing
    // Helper type trait to check if T has a member named cur_bytes
    template <typename T, typename = void>
        struct has_cur_bytes : std::false_type {};

    template <typename T>
        struct has_cur_bytes<T, std::void_t<decltype(std::declval<T>().cur_bytes)>> : std::true_type {};

    template<typename T, size_t N>
        size_t getArrayLength(T (&)[N]) {
            return N;
        }

    template<typename T>
        inline Pkt& get_elem(T& queue, int32_t idx) 
        {
            return queue.pkts[(queue.head+idx) % getArrayLength(queue.pkts)];
        }

    template<typename T1, typename T2>
        inline bool _dequeue(T1 &queue, T2& elem) 
        {
            if (queue.cur_num == 0) {
                // The queue is empty, cannot dequeue
                return false;
            }
            elem = queue.pkts[queue.head];
            queue.head = (queue.head + 1) % PKT_BUF_LEN;
            queue.cur_num -= 1;
            if constexpr (has_cur_bytes<T1>::value) {
                queue.cur_bytes -= elem.ip_pkt_len;
            }
            return true; 
        }

    // Template specialization declaration - implementation in queue_system.cpp
    template<>
        bool _dequeue(PktBuf &queue, Pkt& elem);

    template<typename T1, typename T2>
        inline bool fetch_elem(T1& queue, int32_t idx, T2& elem) 
        {
            if (queue.cur_num == 0) 
                return false; // The queue is empty, cannot dequeue

            if (idx < 0 || static_cast<uint32_t>(idx) >= queue.cur_num) { 
            #if PRINT_QUEUE_OVERFLOW_LOG
            printf("ERROR: fetch_elem index out of bounds, idx: %d, cur_num: %u\n", idx, queue.cur_num);
            #endif
                return false; 
            }
            elem = queue.pkts[(queue.head+idx) % PKT_BUF_LEN];
            return true; 
        }

    template<typename T1, typename T2>
        inline void _enqueue(T1 &queue, T2 pkt) {
            if (queue.cur_num >= PKT_BUF_LEN) {
                #if PRINT_QUEUE_OVERFLOW_LOG
                printf("queue is overflow, cur_num: %u, MAX_PKT_BUF_LEN: %u\n", queue.cur_num, PKT_BUF_LEN);
                #endif
                return;
            }
            queue.pkts[queue.tail] = pkt;
            queue.tail = (queue.tail + 1) % PKT_BUF_LEN;
            queue.cur_num += 1;
            // we also need to increase the queue bytes counter
            if constexpr (has_cur_bytes<T1>::value) {
                queue.cur_bytes += pkt.ip_pkt_len;
            }
        }

    // Template specialization declaration for PktBuf in switch - implementation in queue_system.cpp
    template<>
        void _enqueue(PktBuf &queue, Pkt pkt);

    template<typename T>
        inline uint32_t get_queue_len(T &queue) {
            return queue.cur_num;
        }

    template<typename T>
        inline void clear_queue(T &queue) {
            queue.head = 0;
            queue.tail = 0;
            queue.cur_num = 0;
            if constexpr (has_cur_bytes<T>::value) {
                queue.cur_bytes = 0;
            }
        }

    // remove the range of queue between start and end
    template<typename T1, typename T2>
        inline void rm_queue_range(T1 &queue, T2 pkt, uint16_t start, uint16_t end) {
            for (uint16_t i = start; i < end; i++) {
                _dequeue(queue, pkt);
            }
        }

    template<typename T>
        inline int calculate_queue_length_on_enqueue(T & queue, uint32_t enqueue_time){
            // when consider the influence of PFC PAUSE
            // if this egress port queue is paused, all packets is in the queue, hence, the queue length on enqueue is the total length of packets in the queue
            if(queue.pfc_state == PFCState::PAUSE){
                return queue.queue_bytes_on_enqueue;
            }
            // enqueue_index should not bigger than queue tail
            while(queue.enqueue_index != queue.tail && queue.pkts[queue.enqueue_index].dequeue_time < enqueue_time){
                queue.queue_bytes_on_enqueue -= queue.pkts[queue.enqueue_index].ip_pkt_len;
                queue.enqueue_index++;
                queue.enqueue_index %= PKT_BUF_LEN;
            }
            return queue.queue_bytes_on_enqueue;
        }

    // this function is used to reset the queue length on enqueue when the PFC state is PAUSE
    // since in PFC PAUSE state, the queue length on enqueue is not correct
    template<typename T>
        inline void reset_queue_length_on_enqueue_when_pfc_pause(T & queue){
             // enqueue_index should not bigger than queue tail
             queue.queue_bytes_on_enqueue = queue.cur_bytes;
             queue.enqueue_index = queue.head;
        }

    // this function is used to recalculate the dequeue time of packets in the queue when the PFC state is RESUME
    template<typename T>
        inline void recalculate_queue_length_on_dequeue_when_pfc_resume(T & queue, uint32_t pfc_resume_time, LinkRate &_link_rate){
            // based on the pfc resume time, we should recalculate dequeue time of packets in the queue
            for(uint32_t i = queue.head; i != queue.tail; i = (i + 1) % PKT_BUF_LEN){
                // the first packet should transmit at the pfc resume time
                queue.pkts[i].dequeue_time = pfc_resume_time;
                // add the transmission time of the packet
                pfc_resume_time += queue.pkts[i].ip_pkt_len*8*1.0/_link_rate.link_rate*(1000*1000*1000);
            }
        }

    template<typename T>
        inline int calculate_queue_length_on_dequeue(T & queue, uint32_t dequeue_time){
            // dequeue_index should not bigger than queue tail
            while(queue.dequeue_index != queue.tail && queue.pkts[queue.dequeue_index].enqueue_time < dequeue_time){
                queue.queue_bytes_on_dequeue += queue.pkts[queue.dequeue_index].ip_pkt_len;
                queue.dequeue_index++;
                queue.dequeue_index %= PKT_BUF_LEN;
            }
            return queue.queue_bytes_on_dequeue;
        }

    template<typename T1>
        inline Pkt get_last_packet(T1 & queue){
            int last_index = (queue.tail-1+PKT_BUF_LEN) % PKT_BUF_LEN;
            return queue.pkts[last_index];
        }

    template<typename T1, typename T2>
        inline bool _dequeue_flow(T1& queue, T2& elem) 
        {
            if (queue.cur_num == 0) {
                // The queue is empty, cannot dequeue
                return false;
            }
            elem = queue.flow[queue.head];
            queue.head = (queue.head + 1) % MAX_NET_NPU_FLOW_NUM;
            queue.cur_num -= 1;

            return true; 
        }

    template<typename T1, typename T2>
        inline void _enqueue_flow(T1 &queue, T2 pkt) {
            if (queue.cur_num >= MAX_NET_NPU_FLOW_NUM) {
                // printf("error: flow queue is overloaded\n");
            }
            queue.flow[queue.tail] = pkt;
            queue.tail = (queue.tail + 1) % MAX_NET_NPU_FLOW_NUM;
            queue.cur_num += 1;
        }

    void update_bitmap(RecvBitmapAndTriggerCondition &_bitmap_and_trigger, Pkt &pkt);
    
    void move_recv_bitmap_head_pos(RecvBitmapAndTriggerCondition &_bitmap_and_trigger_condition);

    // Function declaration - implementation in queue_system.cpp  
    void insertionSort(PktBuf &data); 
}
