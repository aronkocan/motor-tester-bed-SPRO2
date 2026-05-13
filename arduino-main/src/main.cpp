#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>


enum class MainState {
    WAIT_FOR_SETUP,
    PREPARE_TEST,
    RUN_MEASUREMENT_CYCLE,
    EVALUATE_NEXT_STEP,
    OUTPUT_RESULTS,
    ERROR
};

enum class MeasurementMode {
    MANUAL,
    AUTO
};



void intialize_hardware(void);

int main(void) {

    intialize_hardware();

    while(1) {
        runStateMachine();
    }
    return 0;
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