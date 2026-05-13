#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>



// =======================
// Runtime States
// =======================

enum class MainState {
    WAIT_FOR_SETUP,
    PREPARE_TEST,
    RUN_MEASUREMENT_CYCLE,
    EVALUATE_NEXT_STEP,
    OUTPUT_RESULTS,
    ERROR
};


// =======================
// Measurement Modes
// =======================

enum class MeasurementMode {
    AUTOMATIC,
    MANUAL_TARGET
};


// =======================
// Setup / Loop
// =======================

void setup() {
    initializeSystem();
}

void loop() {
    runStateMachine();
}


void runStateMachine() {
    switch (currentState) {

        case MainState::WAIT_FOR_SETUP:
            handleWaitForSetupState();
            break;

        case MainState::PREPARE_TEST:
            handlePrepareTestState();
            break;

        case MainState::RUN_MEASUREMENT_CYCLE:
            handleRunMeasurementCycleState();
            break;

        case MainState::EVALUATE_NEXT_STEP:
            handleEvaluateNextStepState();
            break;

        case MainState::OUTPUT_RESULTS:
            handleOutputResultsState();
            break;

        case MainState::ERROR:
            handleErrorState();
            break;
    }
}


// =======================
// State Handlers
// =======================


void handleWaitForSetupState() {

}


void handlePrepareTestState() {

}


void handleRunMeasurementCycleState() {

}


void handleEvaluateNextStepState() {

}


void handleOutputResultsState() {

}


void handleErrorState() {

}


// =======================
// Function Implementations
// =======================

void initializeSystem() {

}