#include "queue_system.hpp"
#include "packet_system.hpp"
#include "packet_system.hpp"
#include <cstdint>
#include <cstdio>

namespace madEscape {
    //random generator (Linear Congruential Generator)
    LCG::LCG(unsigned long _a, unsigned long _c, unsigned long _modulus, unsigned long _seed)
        : a(_a), c(_c), modulus(_modulus), seed(_seed) {}

    unsigned long LCG::next() {
        seed = (a * seed + c) % modulus;
        return seed;
    }

    void LCG::set_seed(unsigned long _seed) {
        seed = _seed;
    }

    // Template specialization for PktBuf dequeue
    template<>
        bool _dequeue(PktBuf &queue, Pkt& elem) 
        {
            if (queue.cur_num == 0) {
                // The queue is empty, cannot dequeue
                return false;
            }
            elem = queue.pkts[queue.head];
            queue.head = (queue.head + 1) % PKT_BUF_LEN;
            queue.cur_num -= 1;

            queue.cur_bytes -= elem.ip_pkt_len;
            queue.queue_bytes_on_dequeue -= elem.ip_pkt_len;



            return true; 
        }

    // Template specialization for PktBuf enqueue
    template<>
        void _enqueue(PktBuf &queue, Pkt pkt) {
            //std::cout << "queue " << &queue <<" current num " << queue.cur_num << std::endl;
            if (queue.cur_num >= PKT_BUF_LEN) {
                printf("queue.cur_num is %u, queue is overloaded\n", queue.cur_num);
                PrintPkt(pkt, "overloaded pkt: ");
            }
            queue.pkts[queue.tail] = pkt;
            queue.tail = (queue.tail + 1) % PKT_BUF_LEN;
            queue.cur_num += 1;
            // we also need to increase the queue bytes counter
            queue.cur_bytes += pkt.ip_pkt_len;
            queue.queue_bytes_on_enqueue += pkt.ip_pkt_len;
          
            assert(queue.cur_num <= PKT_BUF_LEN);
        }
        
    

    void update_bitmap(RecvBitmapAndTriggerCondition &_bitmap_and_trigger, Pkt &pkt){ //ref
        int bitmap_pos = pkt.sq_num / (MTU-pkt.header_len) % PKT_BUF_LEN;
        // Save current tail pointer (right boundary of receive window, points to next expected maximum sequence position)
        uint16_t tail = _bitmap_and_trigger.tail;
        // Save current head pointer (left boundary of receive window, points to minimum sequence position of contiguously received packets)
        uint16_t head = _bitmap_and_trigger.head;
        //update cache_hole_depth and tail position
        // Case 1: No wrap-around (tail >= head) and packet is beyond current window (bitmap_pos >= tail or bitmap_pos < head)
        // This means we received an out-of-order packet that extends the window
        if((tail >= head) && (bitmap_pos >= tail || bitmap_pos < head)){
            _bitmap_and_trigger.cache_hole_depth += (bitmap_pos - tail + PKT_BUF_LEN) % PKT_BUF_LEN;
            _bitmap_and_trigger.tail = (bitmap_pos + 1) % PKT_BUF_LEN;  
        }
        // Case 2: No wrap-around (tail >= head) and packet is within current window (head <= bitmap_pos < tail)
        // This fills a hole in the existing window
        else if((tail >= head) && (bitmap_pos < tail && bitmap_pos >= head)) {
            if((_bitmap_and_trigger.bitmap[bitmap_pos] & 0x80000000) == 0){
                _bitmap_and_trigger.cache_hole_depth--;
            }
        } 
        // Case 3: Wrap-around occurred (tail < head) and packet is beyond current window (tail <= bitmap_pos < head)
        // This extends the window in wrapped state
        else if((bitmap_pos >= tail && bitmap_pos < head)){
            _bitmap_and_trigger.cache_hole_depth += (bitmap_pos - tail + PKT_BUF_LEN) % PKT_BUF_LEN;
            _bitmap_and_trigger.tail = (bitmap_pos + 1) % PKT_BUF_LEN;
        // Case 4: Wrap-around occurred (tail < head) and packet is within current window (bitmap_pos >= head or bitmap_pos < tail)
        // This fills a hole in the wrapped window
        }
        else{
            if((_bitmap_and_trigger.bitmap[bitmap_pos] & 0x80000000) == 0){
                _bitmap_and_trigger.cache_hole_depth--;
            }
        }
        //update bitmap
        _bitmap_and_trigger.bitmap[bitmap_pos] = (1 << 31) | (pkt.sq_num & 0x7FFFFFFF);

        //update bitmap_value
        _bitmap_and_trigger.bitmap_value |= 1ULL << ((bitmap_pos - _bitmap_and_trigger.head + PKT_BUF_LEN) % PKT_BUF_LEN);
        
        move_recv_bitmap_head_pos(_bitmap_and_trigger);
    }

    void move_recv_bitmap_head_pos(RecvBitmapAndTriggerCondition &_bitmap_and_trigger){
        while ((_bitmap_and_trigger.tail - _bitmap_and_trigger.head + PKT_BUF_LEN) % PKT_BUF_LEN > 2)
        {
            uint32_t head_bitmap_value = (_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head] & 0x80000000) >> 31;
            uint32_t head_next_bitmap_value = (_bitmap_and_trigger.bitmap[(_bitmap_and_trigger.head + 1) % PKT_BUF_LEN] & 0x80000000) >> 31;
            if((head_bitmap_value == 1) && (head_next_bitmap_value == 1)) {
                _bitmap_and_trigger.bitmap[_bitmap_and_trigger.head] = 0;
                _bitmap_and_trigger.head =  (_bitmap_and_trigger.head + 1) % PKT_BUF_LEN;
                _bitmap_and_trigger.bitmap_value = _bitmap_and_trigger.bitmap_value >> 1;
            }
            else {
                break;
            }
        }
    }

    void insertionSort(PktBuf &data) {
        int32_t i, j;
        Pkt key;
        uint32_t len = get_queue_len(data);
        for (i = 1; i < len; i++) {
            key = get_elem(data, i);
            j = i - 1;
            // Move elements of data[0..i-1] that are greater than key.enqueue_time to one position ahead of their current position
            while (j >= 0 && get_elem(data, j).enqueue_time > key.enqueue_time) {
                get_elem(data, j+1) = get_elem(data, j);
                j = j - 1;
            }
            get_elem(data, j+1) = key;
        }
    } 
}
