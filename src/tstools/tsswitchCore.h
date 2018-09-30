//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2018, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  Input switch (tsswitch) core engine.
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsswitchOptions.h"
#include "tsswitchInputExecutor.h"
#include "tsswitchOutputExecutor.h"
#include "tsMutex.h"
#include "tsCondition.h"

namespace ts {
    //!
    //! Input switch (tsswitch) namespace.
    //!
    namespace tsswitch {
        //!
        //! Input switch (tsswitch) core engine.
        //! @ingroup plugin
        //!
        class Core
        {
        public:
            //!
            //! Constructor.
            //! @param [in,out] opt Command line options.
            //! @param [in,out] log Log report.
            //!
            Core(Options& opt, Report& log);

            //!
            //! Destructor.
            //!
            ~Core();

            //!
            //! Start the @c tsswitch processing.
            //! @return True on success, false on error.
            //!
            bool start();

            //!
            //! Stop the @c tsswitch processing.
            //! @param [in] success False if the stop is triggered by an error.
            //!
            void stop(bool success);

            //!
            //! Wait for completion of all plugin threads.
            //!
            void waitForTermination();

            //!
            //! Switch to another input plugin.
            //! @param [in] pluginIndex Index of the new input plugin.
            //!
            void setInput(size_t pluginIndex);

            //!
            //! Switch to the next input plugin.
            //!
            void nextInput();

            //!
            //! Switch to the previous input plugin.
            //!
            void previousInput();

            //!
            //! Called by an input plugin when it started an input session.
            //! @param [in] pluginIndex Index of the input plugin.
            //! @param [in] success True if the start operation succeeded.
            //! @return False when @c tsswitch is terminating.
            //!
            bool inputStarted(size_t pluginIndex, bool success);

            //!
            //! Called by an input plugin when it received input packets.
            //! @param [in] pluginIndex Index of the input plugin.
            //! @return False when @c tsswitch is terminating.
            //!
            bool inputReceived(size_t pluginIndex);

            //!
            //! Called by an input plugin when it stopped an input session.
            //! @param [in] pluginIndex Index of the input plugin.
            //! @param [in] success True if the stop operation succeeded.
            //! @return False when @c tsswitch is terminating.
            //!
            bool inputStopped(size_t pluginIndex, bool success);

            //!
            //! Called by the output plugin when it needs some packets to output.
            //! Wait until there is some packets to output.
            //! @param [out] pluginIndex Returned index of the input plugin.
            //! @param [out] first Returned address of first packet to output.
            //! @param [out] count Returned number of packets to output.
            //! Never zero, except when @c tsswitch is terminating.
            //! @return False when @c tsswitch is terminating.
            //!
            bool getOutputArea(size_t& pluginIndex, TSPacket*& first, size_t& count);

            //!
            //! Called by the output plugin after sending packets.
            //! @param [in] pluginIndex Index of the input plugin from which the packets were sent.
            //! @param [in] count Number of output packets to release.
            //! @return False when @c tsswitch is terminating.
            //!
            bool outputSent(size_t pluginIndex, size_t count);

        private:
            // Upon reception of an event (end of input, remote command, etc), there
            // is a list of actions to execute which depends on the switch policy.
            // Types of actions:
            enum ActionType {
                NONE,          // Nothing to do.
                START,         // Start a plugin.
                WAIT_STARTED,  // Wait for start completion of a plugin.
                WAIT_INPUT,    // Wait for input packets on a plugin.
                STOP,          // Stop a plugin.
                WAIT_STOPPED,  // Wait for stop completion of a plugin.
                NOTIF_CURRENT, // Notify a plugin it is the current one (or not).
                SET_CURRENT,   // Set current plugin index.
            };

            // Description of an action with its parameters.
            class Action: public StringifyInterface
            {
            public:
                ActionType type;   // Action to execute.
                size_t     index;  // Input plugin index.
                bool       flag;   // Boolean parameter (depends on the action).

                // Constructor.
                Action(ActionType t = NONE, size_t i = 0, bool f = false) : type(t), index(i), flag(f) {}

                // Copy constructor, changing the flag.
                Action(const Action& other, bool f) : type(other.type), index(other.index), flag(f) {}

                // Implement StringifyInterface.
                virtual UString toString() const override;

                // Operator "less" for containers.
                bool operator<(const Action& a) const;
            };

            typedef std::set<Action> ActionSet;
            typedef std::deque<Action> ActionQueue;

            Options&            _opt;        // Command line options.
            Report&             _log;        // Asynchronous log report.
            InputExecutorVector _inputs;     // Input plugins threads.
            OutputExecutor      _output;     // Output plugin thread.
            Mutex               _mutex;      // Global mutex, protect access to all subsequent fields.
            Condition           _gotInput;   // Signaled each time an input plugin reports new packets.
            size_t              _curPlugin;  // Index of current input plugin.
            size_t              _curCycle;   // Current input cycle number.
            volatile bool       _terminate;  // Terminate complete processing.
            ActionQueue         _actions;    // Sequential queue list of actions to execute.
            ActionSet           _events;     // Pending events, waiting to be cleared.

            // Names of actions for debug messages.
            static const Enumeration _actionNames;

            // Change input plugin with mutex already held.
            void setInputLocked(size_t index);

            // Enqueue an action (with mutex already held).
            void enqueue(const Action& action);

            // Execute all commands until one needs to wait (with mutex already held).
            // The event can be used to unlock a wait action.
            void execute(const Action& event = Action());

            // Inaccessible operations.
            Core() = delete;
            Core(const Core&) = delete;
            Core& operator=(const Core&) = delete;
        };
    }
}
