'use strict';

// Layouts binários para uso com Buffer (Node.js FFI sem JSON).

const SimShapeType = Object.freeze({
  SHAPE_CYLINDER: 0,
  SHAPE_BOX: 1,
});

const SimAgentFlags = Object.freeze({
  AGENT_ENABLED: 1 << 0,
  AGENT_VEHICLE: 1 << 1,
  AGENT_TELEPORT: 1 << 2,
  AGENT_ANCHOR: 1 << 3,
  AGENT_ANCHOR_HEADING: 1 << 4,
  AGENT_ANCHOR_VELOCITY: 1 << 5,
});

const Structs = Object.freeze({
  SimAgentDescFFI: {
    size: 64,
    offsets: {
      agentId: 0,
      teamMask: 4,
      avoidMask: 8,
      flags: 12,
      shapeType: 16,
      pos: 20,
      vel: 32,
      headingDeg: 44,
      radius: 48,
      halfX: 52,
      halfZ: 56,
      height: 60,
    },
  },
  SimParamsFFI: {
    size: 72,
    offsets: {
      anchorHeadingAlpha: 56,
      anchorVelAlpha: 60,
      anchorMaxSnapDist: 64,
      anchorMaxSnapYawDeg: 68,
    },
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
