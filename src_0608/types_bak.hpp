#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>
#include <madrona/rand.hpp>
#include <madrona/physics.hpp>
#include <madrona/render/ecs.hpp>

#include "consts.hpp"





#define K_ARY 4 // 8 //4 //16
#define INPORT_NUM  K_ARY //40

#define NIC_RATE 1000LL*1000*1000 * 100 // 100 Gbps
#define BW 100 // 100 Gbps
// egress port
#define QUEUE_NUM 1


// packet buffer
#define PKT_BUF_LEN 50000 //300 // 1024*1
#define TX_BUF_LEN PKT_BUF_LEN

#define BUF_SIZE PKT_BUF_LEN*1000  // 200*1460 //byte
#define PFC_THR 80

// ECN configuration
#define K_MAX 1000 * 1024   // 400 * 4 * 1024
#define K_MIN 200 * 1024   // 100 * 4 * 1024
#define P_MAX 0.2

//link
#define SS_LINK_DELAY 1000 // 1000 //ns switch --> switch
#define HS_LINK_DELAY 1000  // 1000  //ns host --> switch
#define MTU 1500

#define LOOKAHEAD_TIME SS_LINK_DELAY

#define PACKET_PROCESS_TIME 100 //100 ns
#define TMP_BUF_LEN SS_LINK_DELAY/PACKET_PROCESS_TIME

//NPU
#define MAX_NPU_FLOW_NUM 2000 //max number of flows concurrent exist in 1 npu at most 

// // NIC
// #define MAX_NIC_FLOW_NUM 100

// number of dst host
#define NUM_HOST K_ARY*K_ARY*K_ARY/4
#define MAX_NUM_NEXT_HOP K_ARY
//

#define MAX_PATH_LEN 20 //16 //8 hops (each hop includes device id and port id)

//Retransmit timer
#define RETRANSMIT_TIMER 100*1000 // 100us

// DCQCN parameter
#define DCQCN_Alpha 1.0
#define DCQCN_CurrentRate  NIC_RATE // 100Gbps 
#define DCQCN_TargetRate  NIC_RATE // 100Gbps
#define DCQCN_G 0.00390625
#define DCQCN_AIRate 20*1000*1000 // 20M bps  //50M bps
#define DCQCN_HAIRate 200*1000*1000 // 200 Mbps  //500M bps
#define DCQCN_AlphaUpdateInterval 1 * 1000 // 1us,  nanoseconds
#define DCQCN_RateDecreaseInterval 4 * 1000 // 4us,  nanoseconds
#define DCQCN_RateIncreaseInterval 300 * 1000 // 300us,  nanoseconds

#define DCQCN_IncreaseStageCount 0 // runtime variables, should set at runtime
#define DCQCN_RecoveryStageThreshhold 1 // 5

#define DCQCN_RateDecreaseNextTime 0 // runtime variables, should set at runtime
#define DCQCN_RateIncreaseNextTime 0 // runtime variables, should set at runtime
#define DCQCN_AlphaUpdateNextTime 0 // runtime variables, should set at runtime
#define DCQCN_RateIsDecreased false

#define CNP_DURATION 50*1000 // 50*1000 //50us

//Map structure size
#define MAP_SIZE 1000

#define MESSAGE_BUFFER_LENGTH (1000*10)






namespace madEscape {

// Include several madrona types into the simulator namespace for convenience
using madrona::Entity;
using madrona::RandKey;
using madrona::CountT;
using madrona::base::Position;
using madrona::base::Rotation;
using madrona::base::Scale;
using madrona::base::ObjectID;
using madrona::phys::Velocity;
using madrona::phys::ResponseType;
using madrona::phys::ExternalForce;
using madrona::phys::ExternalTorque;
using madrona::phys::RigidBody;

// WorldReset is a per-world singleton component that causes the current
// episode to be terminated and the world regenerated
// (Singleton components like WorldReset can be accessed via Context::singleton
// (eg ctx.singleton<WorldReset>().reset = 1)
struct WorldReset {
    int32_t reset;
};

// Discrete action component. Ranges are defined by consts::numMoveBuckets (5),
// repeated here for clarity
struct Action {
    int32_t moveAmount; // [0, 3]
    int32_t moveAngle; // [0, 7]
    int32_t rotate; // [-2, 2]
    int32_t grab; // 0 = do nothing, 1 = grab / release
};

// Per-agent reward
// Exported as an [N * A, 1] float tensor to training code
struct Reward {
    float v;
};

// Per-agent component that indicates that the agent's episode is finished
// This is exported per-agent for simplicity in the training code
struct Done {
    // Currently bool components are not supported due to
    // padding issues, so Done is an int32_t
    int32_t v;
};

// Observation state for the current agent.
// Positions are rescaled to the bounds of the play area to assist training.
struct SelfObservation {
    float roomX;
    float roomY;
    float globalX;
    float globalY;
    float globalZ;
    float maxY;
    float theta;
    float isGrabbing;
};

// The state of the world is passed to each agent in terms of egocentric
// polar coordinates. theta is degrees off agent forward.
struct PolarObservation {
    float r;
    float theta;
};

struct PartnerObservation {
    PolarObservation polar;
    float isGrabbing;
};

// Egocentric observations of other agents
struct PartnerObservations {
    PartnerObservation obs[consts::numAgents - 1];
};

// PartnerObservations is exported as a
// [N, A, consts::numAgents - 1, 3] // tensor to pytorch
static_assert(sizeof(PartnerObservations) == sizeof(float) *
    (consts::numAgents - 1) * 3);

// Per-agent egocentric observations for the interactable entities
// in the current room.
struct EntityObservation {
    PolarObservation polar;
    float encodedType;
};

struct RoomEntityObservations {
    EntityObservation obs[consts::maxEntitiesPerRoom];
};

// RoomEntityObservations is exported as a
// [N, A, maxEntitiesPerRoom, 3] tensor to pytorch
static_assert(sizeof(RoomEntityObservations) == sizeof(float) *
    consts::maxEntitiesPerRoom * 3);

// Observation of the current room's door. It's relative position and
// whether or not it is ope
struct DoorObservation {
    PolarObservation polar;
    float isOpen; // 1.0 when open, 0.0 when closed.
};

struct LidarSample {
    float depth;
    float encodedType;
};

// Linear depth values and entity type in a circle around the agent
struct Lidar {
    LidarSample samples[consts::numLidarSamples];
};

// Number of steps remaining in the episode. Allows non-recurrent policies
// to track the progression of time.
struct StepsRemaining {
    uint32_t t;
};

// Tracks progress the agent has made through the challenge, used to add
// reward when more progress has been made
struct Progress {
    float maxY;
};

// Per-agent component storing Entity IDs of the other agents. Used to
// build the egocentric observations of their state.
struct OtherAgents {
    madrona::Entity e[consts::numAgents - 1];
};

// Tracks if an agent is currently grabbing another entity
struct GrabState {
    Entity constraintEntity;
};

// This enum is used to track the type of each entity for the purposes of
// classifying the objects hit by each lidar sample.
enum class EntityType : uint32_t {
    None,
    Button,
    Cube,
    Wall,
    Agent,
    Door,
    NumTypes,
};

// A per-door component that tracks whether or not the door should be open.
struct OpenState {
    bool isOpen;
};

// Linked buttons that control the door opening and whether or not the door
// should remain open after the buttons are pressed once.
struct DoorProperties {
    Entity buttons[consts::maxEntitiesPerRoom];
    int32_t numButtons;
    bool isPersistent;
};

// Similar to OpenState, true during frames where a button is pressed
struct ButtonState {
    bool isPressed;
};

// Room itself is not a component but is used by the singleton
// component "LevelState" (below) to represent the state of the full level
struct Room {
    // These are entities the agent will interact with
    Entity entities[consts::maxEntitiesPerRoom];

    // The walls that separate this room from the next
    Entity walls[2];

    // The door the agents need to figure out how to lower
    Entity door;
};

// A singleton component storing the state of all the rooms in the current
// randomly generated level
struct LevelState {
    Room rooms[consts::numRooms];
};

/* ECS Archetypes for the game */






//
struct Results {
    uint32_t results;
};
struct Results2 {
   int32_t encoded_string[1000];
};

struct  MadronaEvent
{
    int32_t type;
    int32_t eventId;
    int32_t time;
    int32_t src;
    int32_t dst;
    int32_t size;
    int32_t port;
};

struct MadronaEventsQueue
{
    int32_t events[MESSAGE_BUFFER_LENGTH];
};

struct MadronaEvents
{
    int32_t events[MESSAGE_BUFFER_LENGTH];
};

struct  MadronaEventsResult
{
    int32_t events[MESSAGE_BUFFER_LENGTH];
};

struct  Configuration
{
    int32_t events[1000];
};

struct  ProcessParams
{
    int32_t params[1000];
};

struct CurStep {
    uint32_t step;
};

struct  SimulationTime
{
    int64_t time;
};


//






// There are 2 Agents in the environment trying to get to the destination
struct Agent : public madrona::Archetype<
    // RigidBody is a "bundle" component defined in physics.hpp in Madrona.
    // This includes a number of components into the archetype, including
    // Position, Rotation, Scale, Velocity, and a number of other components
    // used internally by the physics.
    RigidBody,

    // Internal logic state.
    GrabState,
    Progress,
    OtherAgents,
    EntityType,

    // Input
    Action,

    // Observations
    SelfObservation,
    PartnerObservations,
    RoomEntityObservations,
    DoorObservation,
    Lidar,
    StepsRemaining,

    // Reward, episode termination
    Reward,
    Done,

    // Visualization: In addition to the fly camera, src/viewer.cpp can
    // view the scene from the perspective of entities with this component
    madrona::render::RenderCamera,
    // All entities with the Renderable component will be drawn by the
    // viewer and batch renderer
    madrona::render::Renderable,



// //  
    CurStep,
    Results,
    Results2,
    SimulationTime,
    MadronaEventsQueue,
    MadronaEventsResult,
    MadronaEvents,
    ProcessParams
// //

> {};



// Archetype for the doors blocking the end of each challenge room
struct DoorEntity : public madrona::Archetype<
    RigidBody,
    OpenState,
    DoorProperties,
    EntityType,
    madrona::render::Renderable
> {};

// Archetype for the button objects that open the doors
// Buttons don't have collision but are rendered
struct ButtonEntity : public madrona::Archetype<
    Position,
    Rotation,
    Scale,
    ObjectID,
    ButtonState,
    EntityType,
    madrona::render::Renderable
> {};

// Generic archetype for entities that need physics but don't have custom
// logic associated with them.
struct PhysicsEntity : public madrona::Archetype<
    RigidBody,
    EntityType,
    madrona::render::Renderable
> {};

















//********************************************************

struct AJLink {
    uint32_t aj_link[100000][5];
    uint32_t link_num;
    uint32_t npu_num;
};


struct EgressPortID {
   uint32_t egress_port_id;
};

enum class NextHopType : uint8_t {
    HOST = 0,
    SWITCH = 1,
};

struct NextHop {
    uint32_t next_hop;
};

struct HSLinkDelay {
    int64_t HS_link_delay;
};

struct NICRate {
    int64_t nic_rate;
};

enum class PktType : uint8_t {
    DATA = 0,
    ACK = 1,
    PFC_PAUSE = 2,
    PFC_RESUME = 3,
    NACK = 4,
    CNP = 5,
};

enum class ECN_MARK: uint8_t {
    NO = 0,
    YES = 1,
};

struct Pkt {
    uint32_t src;   
    uint32_t dst;  // also PFC_FOR_UPSTREAM for the PFC frame 
    uint16_t l4_port;
    uint16_t header_len;
    uint16_t payload_len;
    uint16_t ip_pkt_len;
    int64_t sq_num;
    uint16_t flow_id;  // also the idx of priority queue for the PFC frame 
    int64_t enqueue_time;
    int64_t dequeue_time;
    uint8_t flow_priority;
    PktType pkt_type;  // also PFC state for the PFC frame 

    uint32_t path[MAX_PATH_LEN];
    uint8_t path_len;

    ECN_MARK ecn;
};
struct PktBuf {
    Pkt pkts[PKT_BUF_LEN];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num;
    uint32_t cur_bytes;
};

struct AckPktBuf {
    Pkt pkts[PKT_BUF_LEN];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num;
    uint32_t cur_bytes;
};


enum class PFCState : uint8_t {
    PAUSE = 2,
    RESUME = 3,
};
struct PktQueue {
    PktBuf pkt_buf[QUEUE_NUM];
    PFCState queue_pfc_state[QUEUE_NUM];
};


struct SimTime {
    int64_t sim_time;
};
struct SimTimePerUpdate {
    int64_t sim_time_per_update;
};

enum class FlowState : uint8_t {
    UNCOMPLETE = 0,
    COMPLETE = 1,
};


struct FlowID {
    uint16_t flow_id;
};


//////////////////////////////////////////////////////////////////////////////////////////////
// support multiple flow for a host/nic

// api for Astra-Sim system layer
struct FlowEvent {
    // Entity flow_entt;
    uint16_t flow_id;
    uint32_t src;
    uint32_t dst;
    uint16_t l4_port;
    uint32_t nic; // nic id
    int64_t flow_size;
    int64_t start_time;
    int64_t stop_time;
    // uint16_t phase_num; // which step in a ring all-reduce    
    FlowState flow_state;
    int64_t extra_1;
};


struct CompletedFlowQueue {
    FlowEvent flow[MAX_NPU_FLOW_NUM];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num; //flow_num
    // uint32_t cur_bytes;
};

struct NewFlowQueue {
    FlowEvent flow[MAX_NPU_FLOW_NUM];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num; //flow_num
    // uint32_t cur_bytes;
};


struct Src {
    uint32_t src;
};

struct Dst {
    uint32_t dst;
};

struct L4Port {
    uint16_t l4_port;
};

struct FlowSize {
    int64_t flow_size;
};

struct StartTime {
    int64_t start_time;

};

struct StopTime {
    int64_t stop_time;

};

struct NIC_ID {
    uint32_t nic_id; 
};

struct SndServerID {
    uint32_t snd_server_id; 
};

struct RecvServerID {
    uint32_t recv_server_id; 
};


// struct SqNum {
//     int64_t sq_num;
// };

struct SndNxt {
    int64_t snd_nxt;
};

struct SndUna {
    int64_t snd_una;
};

struct LastAckTimestamp {
    int64_t last_ack_timestamp;
};

struct NxtPktEvent {
    int64_t nxt_pkt_event;
};

struct CC_Para {
    int64_t m_rate; // current rate/Mpbs
    int64_t tar_rate; // target rate/Mbps
    double dcqcn_Alpha; // the alpha parameter in DCQCN
    double dcqcn_G; // the G parameter in DCQCN
    int64_t dcqcn_AIRate; // the increase rate in additive increase stage
    int64_t dcqcn_HAIRate; // the increase rate in hyper additive increase stage
    uint32_t dcqcn_AlphaUpdateInterval; // the timer interval to update alpha
    uint32_t dcqcn_RateDecreaseInterval; // the timer interval to decrease target rate
    uint32_t dcqcn_RateIncreaseInterval; // the timer interval to increase target rate
    uint32_t dcqcn_IncreaseStageCount; // record the count increase stage
    uint32_t dcqcn_RecoveryStageThreshold; // the threshshold of reconvery stage
    int64_t dcqcn_AlphaUpdateNextTime; // the next time (nanoseconds) to update alpha parameter
    int64_t dcqcn_RateDecreaseNextTime; // the next time (nanoseconds) to decrease target rate
    int64_t dcqcn_RateIncreaseNextTime; // the next time (nanoseconds) to increase target rate
    bool dcqcn_RateIsDecreased; // the flag to indicate rate is decreased when received ECE flag
    bool CNPState; 
};

struct Extra_1 {
    int64_t extra_1;
};



struct SndFlow : public madrona::Archetype<
    FlowID,
    Src, // src npu
    Dst, // dst npu
    L4Port, // layer 4 port

    FlowSize,
    StartTime,
    StopTime,
    
    NIC_ID, // nic id
    SndServerID, // host pair id
    RecvServerID, // host pair id

    // SqNum,
    SndNxt,
    SndUna,
    FlowState,

    //cc
    LastAckTimestamp,
    NxtPktEvent,
    PFCState,
    CC_Para,

    PktBuf,
    AckPktBuf,
    
    NICRate,
    HSLinkDelay,
    SimTime,
    SimTimePerUpdate,

    Extra_1
> {};


struct LastCNPTimestamp {
    int64_t last_cnp_timestamp;
};

struct RecvBytes {
    int64_t recv_bytes;
};

struct RecvFlow : public madrona::Archetype<
    FlowID,
    Src, // src npu
    Dst, // dst npu
    L4Port, // layer 4 port
    
    FlowSize,
    StartTime,
    StopTime,
 
    NIC_ID, // nic id
    SndServerID, // host pair id
    RecvServerID, // host pair id

    FlowState,

    LastCNPTimestamp,
    RecvBytes,

    PktBuf,

    NICRate,
    HSLinkDelay,
    SimTime,
    SimTimePerUpdate    
> {};


struct SendFlows {
    madrona::Entity flow[MAX_NPU_FLOW_NUM];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num;
};


struct NPU_ID {
    uint32_t npu_id; 
};

struct NPU : public madrona::Archetype<
    NPU_ID, //
    SimTime,
    SimTimePerUpdate,
    SendFlows,
    NewFlowQueue, // from Asstra-sim system layer
    CompletedFlowQueue // to Asstra-sim system layer
> {};
//////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////
// NIC entity archetype
struct MountedFlows {
    madrona::Entity flow[MAX_NPU_FLOW_NUM];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num;
};
// struct MountedFlows {
//     Map<uint32_t, Entity> flow;
// };


struct BidPktBuf {
    PktBuf snd_buf;
    PktBuf recv_buf;
};

struct TXElem {
    int64_t dequeue_time;
    uint16_t ip_pkt_len;
};
struct TXHistory {
    TXElem pkts[TX_BUF_LEN];
    uint16_t head;
    uint16_t tail;
    uint16_t cur_num;
    uint32_t cur_bytes;
};

struct Seed {
    uint32_t seed;
};


struct NIC : public madrona::Archetype<
    NIC_ID,
    NICRate,
    HSLinkDelay,
    SimTime,
    SimTimePerUpdate,
    MountedFlows,
    BidPktBuf,
    TXHistory,
    NextHopType,
    NextHop,
    Seed
> {};
///////////////////////////////////////////////////////////////////////////////////////////////


struct IngressPortID {
   uint32_t ingress_port_id;
};

enum class PortType : uint8_t {
    InPort = 0,
    EPort = 1,
};

// for the router forwarding system, of GPU_acclerated DES
struct LocalPortID {
   uint32_t local_port_id;
};
struct GlobalPortID {
   uint32_t global_port_id;
};


enum class SchedTrajType : uint8_t {
    SP = 0,
    FIFO = 1,
};


enum class SwitchType : uint8_t {
    Edge = 0,
    Aggr = 1,
    Core = 2,
};

struct SwitchID {
    uint32_t switch_id;
};


struct ForwardPlan {
    uint64_t forward_plan[INPORT_NUM];
};


struct FIBTable {
    uint8_t fib_table[NUM_HOST][MAX_NUM_NEXT_HOP];
};


struct LinkRate {
    int64_t link_rate;
};

struct SSLinkDelay {
    int64_t SS_link_delay;
};

struct QueueNumPerPort {
    uint8_t queue_num_per_port;
};


struct Switch : public madrona::Archetype<
    SwitchType, //
    SwitchID, //
    FIBTable,
    QueueNumPerPort
> {};


struct IngressPort : public madrona::Archetype<
    PortType,
    LocalPortID,
    GlobalPortID,
    SwitchID, //
    PktBuf, //
    ForwardPlan, //
    SimTime,
    SimTimePerUpdate
> {};


struct EgressPort : public madrona::Archetype<
    SchedTrajType,
    PortType,
    LocalPortID,
    GlobalPortID,
    SwitchID,
    PktQueue,
    TXHistory,
    NextHopType,
    NextHop,
    LinkRate,
    SSLinkDelay,
    SimTime,
    SimTimePerUpdate,
    Seed
> {};




}
