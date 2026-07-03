import time
from typing import Mapping

from numpy.typing import NDArray as Array

import pyspectrum3 as ps3

list = []


def on_frame(data):
    list.append(data)
    # только для дебага, print довольно прожорливая функция (либо можно увеличить READ_BUFFER в вкшмукючьд)
    # в data.result хранится кадр только с одной сборки. Для склеивания кадров можно использовать data.assembly_params.id и data.frame_state.frame_number
    print(f"{data.assembly_params.id} {data.frame_state.frame_number}: [{data.result[0]} {data.result[1]} {data.result[2]} ... {data.result[len(data.result) - 3]} {data.result[len(data.result) - 2]} {data.result[len(data.result) - 1]}]")


def on_status(status: Mapping[str, ps3.AssemblyStatus]):
    print(status, flush=True)


def log(msg):
    print(msg)


def on_error(error: ps3.AsyncDriverException):
    print(error.get_messages())


if __name__ == '__main__':
    dev = ps3.DeviceManager()
    ps3.DriverConfig.parse("driver.xml")
    cfg = ps3.AutoConfig().config()
    dev.initialize(cfg)

    dev.set_context_callback(on_frame)
    #dev.set_status_callback(on_status)
    dev.set_error_callback(on_error)
    #dev.set_log_callback(log)

    dev.connect()
    dev.set_pipe_filter(ps3.DefaultCopyPipeFilter.instance())

    dev.set_measurement(ps3.Measurement(ps3.Exposure(1000), 10000, 0))
    dev.read()

    #
    while True:
        time.sleep(1)
