#include <blmc_drivers/devices/motor.hpp>
#include <blmc_drivers/devices/analog_sensor.hpp>
#include <blmc_drivers/utils/timer.hpp>
#include <blmc_drivers/devices/leg.hpp>

class Controller
{
private:
    std::shared_ptr<blmc_drivers::Motor> motor_;
    std::shared_ptr<blmc_drivers::AnalogSensor> analog_sensor_;

public:
    Controller(std::shared_ptr<blmc_drivers::Motor> motor,
               std::shared_ptr<blmc_drivers::AnalogSensor> analog_sensor):
        motor_(motor), analog_sensor_(analog_sensor) { }

    void start_loop()
    {
        osi::start_thread(&Controller::loop, this);
    }

private:
    /**
     * @brief this function is just a wrapper around the actual loop function,
     * such that it can be spawned as a posix thread.
     */
    static THREAD_FUNCTION_RETURN_TYPE loop(void* instance_pointer)
    {
        ((Controller*)(instance_pointer))->loop();
    }


    /**
     * @brief this is a simple control loop which runs at a kilohertz.
     *
     * it reads the measurement from the analog sensor, in this case the
     * slider. then it scales it and sends it as the current target to
     * the motor.
     */
    void loop()
    {
        Timer<10> time_logger("controller");
        while(true)
        {
            double analog_measurement =
                    analog_sensor_->get_measurement()->newest_element();
            double current_target = 4 * (analog_measurement - 0.5);

            motor_->set_current_target(current_target);
            motor_->send_if_input_changed();

            // print -----------------------------------------------------------
            Timer<>::sleep_ms(1);
            time_logger.end_and_start_interval();
            if ((time_logger.count() % 1000) == 0)
            {
                rt_printf("sending current: %f\n", current_target);
                // time_logger.print_status();
            }
        }
    }
};

class LegController
{
private:
    std::shared_ptr<blmc_drivers::Leg> leg_;
    std::shared_ptr<blmc_drivers::AnalogSensor> analog_sensor_;

public:
    LegController(std::shared_ptr<blmc_drivers::Leg> leg,
               std::shared_ptr<blmc_drivers::AnalogSensor> analog_sensor):
        leg_(leg), analog_sensor_(analog_sensor) { }

    void start_loop()
    {
        osi::start_thread(&LegController::loop, this);
    }

private:
    /**
     * @brief this function is just a wrapper around the actual loop function,
     * such that it can be spawned as a posix thread.
     */
    static THREAD_FUNCTION_RETURN_TYPE loop(void* instance_pointer)
    {
        ((LegController*)(instance_pointer))->loop();
    }


    /**
     * @brief this is a simple control loop which runs at a kilohertz.
     *
     * it reads the measurement from the analog sensor, in this case the
     * slider. then it scales it and sends it as the current target to
     * the motor.
     */
    void loop()
    {
        Timer<10> time_logger("Leg controller");
        while(true)
        {
            double analog_measurement =
                    analog_sensor_->get_measurement()->newest_element();
            double current_target = (analog_measurement - 0.5);

            leg_->set_current_target(current_target, blmc_drivers::Leg::knee);
            leg_->send_if_input_changed();

            // print -----------------------------------------------------------
            Timer<>::sleep_ms(1);
            time_logger.end_and_start_interval();
            if ((time_logger.count() % 1000) == 0)
            {
                rt_printf("sending current: %f\n", current_target);
                // time_logger.print_status();
            }
        }
    }
};




int main(int argc, char **argv)
{  
    osi::initialize_realtime_printing();

    // create bus and boards -------------------------------------------------

    auto can_bus = std::make_shared<blmc_drivers::CanBus>("can1");

    auto board = std::make_shared<blmc_drivers::CanBusMotorBoard>(can_bus);
    // create motors and sensors ---------------------------------------------
    auto motor_hip = std::make_shared<blmc_drivers::Motor>(board,0);
    auto motor_knee = std::make_shared<blmc_drivers::Motor>(board,1); 

    auto leg = std::make_shared<blmc_drivers::Leg>(motor_hip, motor_knee);

    rt_printf("leg is set up \n");

    // auto analog_sensor_1 = std::make_shared<blmc_drivers::AnalogSensor>(board1, 0);
    // auto analog_sensor_2 = std::make_shared<blmc_drivers::AnalogSensor>(board1, 1);
    auto analog_sensor = std::make_shared<blmc_drivers::AnalogSensor>(board, 0);

    rt_printf("sensors are set up \n");

    // Controller controller1(motor_1, analog_sensor_3);
    // Controller controller2(motor_2, analog_sensor_3);

    LegController leg_controller(leg, analog_sensor);
    // Controller controller3(motor_3, analog_sensor_3);
    rt_printf("controllers are set up \n");

    leg_controller.start_loop();
    // controller3.start_loop();

    rt_printf("loops have started \n");

    while(true)
    {
        Timer<>::sleep_ms(10);
    }

    return 0;
}