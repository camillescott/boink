from boink import libboink


typenames = [(libboink.hashing.FwdRollingShifter, 'FwdRollingShifter'),
             (libboink.hashing.CanRollingShifter, 'CanRollingShifter'),
             (libboink.hashing.FwdUnikmerShifter, 'FwdUnikmerShifter'),
             (libboink.hashing.CanUnikmerShifter, 'CanUnikmerShifter')]
types = [_type for _type, _name in typenames]


UKHS = libboink.hashing.UKHS
HashExtender = libboink.hashing.HashExtender
extender_selector_t = libboink.hashing.extender_selector_t

for hasher_t, name in typenames:
    globals()[name] = hasher_t

