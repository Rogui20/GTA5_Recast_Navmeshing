'use strict';

// Layouts binários para uso com Buffer (Node.js FFI sem JSON).

const SimShapeType = Object.freeze({
  SHAPE_CYLINDER: 0,
  SHAPE_BOX: 1,
});

const SimAgentFlags = Object.freeze({
  AGENT_ENABLED: 1 << 0,
  AGENT_VEHICLE: 1 << 1,
});

const Structs = Object.freeze({
  SimAgentDescFFI: {
    size: 52,
    offsets: {
      agentId: 0,
      teamMask: 4,
      avoidMask: 8,
      flags: 12,
      shapeType: 16,
      pos: 20,
      headingDeg: 32,
      radius: 36,
      halfX: 40,
      halfZ: 44,
      height: 48,
    },
  },
  SimParamsFFI: {
    size: 56,
  },
  SimEventFFI: {
    size: 36,
  },
});

module.exports = {
  SimShapeType,
  SimAgentFlags,
  Structs,
};
