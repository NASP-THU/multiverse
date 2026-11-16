#include "congestion_control_system.hpp"

namespace madEscape {
    // DCQCN CC logic
    void dcqcn_rate_increase(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime) 
    {
        #if !PRINT_CC_LOG
        (void)src;
        (void)dst;
        (void)flow_id;
        (void)curSimTime;
        #endif
        if (cc_para.dcqcn_IncreaseStageCount < cc_para.dcqcn_RecoveryStageThreshold)
        {
            // fast recovery stage
            // update current rate
            cc_para.m_rate = cc_para.m_rate / 2 + cc_para.tar_rate / 2;
            #if PRINT_CC_LOG
            printf("In fast recovery stage. sender: %u, receiver: %u, flow id: %u, sim_time: %ld, target rate is %.3lf Gbps, and current rate is %.3lf Gbps\n", src, dst, flow_id, curSimTime, cc_para.tar_rate/(1000*1000*1000.0), cc_para.m_rate/(1000*1000*1000.0));
            #endif
        }
        else if (cc_para.dcqcn_IncreaseStageCount == cc_para.dcqcn_RecoveryStageThreshold)
        {
            // additive increase stage
            // target rate increases with fixed AI sending rate
            cc_para.tar_rate += cc_para.dcqcn_AIRate;
            // if target rate is bigger than the device rate, set to device rate
            if (cc_para.tar_rate > NIC_RATE) {
                cc_para.tar_rate = NIC_RATE;
            }
            // update current rate
            cc_para.m_rate = cc_para.m_rate / 2 + cc_para.tar_rate / 2;
            #if PRINT_CC_LOG
            printf("In additive increase stage. sender: %u, receiver: %u, flow id: %u, sim_time: %ld, target rate is %.3lf Gbps, and current rate is %.3lf Gbps\n", src, dst, flow_id, curSimTime, cc_para.tar_rate/(1000*1000*1000.0), cc_para.m_rate/(1000*1000*1000.0));
            #endif
        }
        else
        {
            // hyper increase stage
            // target rate increases with fixed AI sending rate
            cc_para.tar_rate += cc_para.dcqcn_HAIRate;

            // if target rate is bigger than the device rate, set to device rate
            if (cc_para.tar_rate > NIC_RATE) {
                cc_para.tar_rate = NIC_RATE;
            }
            // update current rate
            cc_para.m_rate = cc_para.m_rate / 2 + cc_para.tar_rate / 2;
            if(cc_para.m_rate > NIC_RATE) {
                cc_para.m_rate = NIC_RATE;
            }
            #if PRINT_CC_LOG
            printf("In hyper increase stage. sender: %u, receiver: %u, flow id: %u, sim_time: %ld, target rate is %.3lf Gbps, and current rate is %.3lf Gbps\n", src, dst, flow_id, curSimTime, cc_para.tar_rate/(1000*1000*1000.0), cc_para.m_rate/(1000*1000*1000.0));
            #endif
        }
        // increase the stage count
        cc_para.dcqcn_IncreaseStageCount++;
    }

    void dcqcn_alpha_update(CC_Para &cc_para)
    {
        // if we receive CNP packet, increase value of alpha
        if (cc_para.CNPState == true)
        {
            cc_para.dcqcn_Alpha = (1 - cc_para.dcqcn_G) * cc_para.dcqcn_Alpha + cc_para.dcqcn_G;
        }
        // else decrease the value of alpha
        else
        {
            cc_para.dcqcn_Alpha = (1 - cc_para.dcqcn_G) * cc_para.dcqcn_Alpha;
        }
    }
    // decrease stage of sending rate in DCQCN, when received ECE packet
    void dcqcn_rate_decrease(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime) {
    #if !PRINT_CC_LOG
        (void)src;
        (void)dst;
        (void)flow_id;
        (void)curSimTime;
        #endif
        if (cc_para.CNPState != true)
            return;
        // record current rate to later fast recovery
        #if PRINT_CC_LOG
        printf(" original rate: %.3lf, dcqcn_Alpha: %.3lf\n", cc_para.m_rate/(1000*1000*1000.0), cc_para.dcqcn_Alpha);
        #endif
        if (cc_para.dcqcn_IncreaseStageCount > 0) {
            cc_para.tar_rate = cc_para.m_rate;
        }
        cc_para.m_rate = (long int)(cc_para.m_rate * (1.0 - cc_para.dcqcn_Alpha / 2.0));
        if (cc_para.m_rate == 0) {
            cc_para.m_rate = 1;
        }
        cc_para.dcqcn_IncreaseStageCount = 0;
        #if PRINT_CC_LOG
        printf("In decrease stage. sender: %u, receiver: %u, flow id: %u, sim_time: %ld, target rate is %.3lf Gbps, and current rate is %.3lf Gbps\n", src, dst, flow_id, curSimTime, cc_para.tar_rate/(1000*1000*1000.0), cc_para.m_rate/(1000*1000*1000.0));
        #endif
    }
    // reset the alpha update and rate increase timer, when rate is decreased
    void dcqcn_timer_reset_if_decrease(CC_Para &cc_para, long curSimTime) {
        // if rate isn't decreased
        if (!cc_para.dcqcn_RateIsDecreased)
            return;
        // since we decrease the rate, we need to reset rate increase timer
        cc_para.dcqcn_AlphaUpdateNextTime = cc_para.dcqcn_RateIncreaseNextTime = curSimTime + cc_para.dcqcn_RateIncreaseInterval;
        cc_para.dcqcn_RateIsDecreased = false;
    }
    void dcqcn_timer_rate_increase(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime) {
        // if current simulation time is smaller than the next rate increase time,
        // should not tigger this function
        if (curSimTime < cc_para.dcqcn_RateIncreaseNextTime)
            return;
        // schedule the next rate increase timer
        cc_para.dcqcn_RateIncreaseNextTime = curSimTime + cc_para.dcqcn_RateIncreaseInterval;
        // increase sending rate
        dcqcn_rate_increase(cc_para, src, dst, flow_id, curSimTime);
    }
    void dcqcn_timer_alpha_update(CC_Para &cc_para, long curSimTime) {
        // if current simulation time is smaller than the next rate decrease time,
        // should not tigger this function
        if (curSimTime < cc_para.dcqcn_AlphaUpdateNextTime)
            return;
        // schedule the next alpha update timer
        cc_para.dcqcn_AlphaUpdateNextTime += cc_para.dcqcn_AlphaUpdateInterval;
        dcqcn_alpha_update(cc_para);
    } 
}
