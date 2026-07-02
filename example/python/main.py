import sys
sys.path.append('cmake-build-debug/binding')
import matplotlib.pyplot as plt
import matplotlib
import pyspectrum2
from time import time, sleep

matplotlib.use("Qt5agg")
figure: plt.Figure
figure, ax = plt.subplots()

dev = pyspectrum2.DeviceManager("10.116.220.2")
frames = []
# n_frames = 0x010203
n_frames = 10000
# n_frames = 1122867

cntr = 0
big_cntr = 0
running = True


def on_close(_):
    global running
    running = False

figure.canvas.mpl_connect('close_event', on_close)

def on_frame(data):
    global cntr, big_cntr
    frames.append(data)
    cntr += 1


def on_status(status):
    global already_running, big_cntr, cntr
    # ждем окончания
    if status.code == pyspectrum2.DeviceStatusCode.DONE_READING:
        print(f'Read {cntr}')
        cntr = 0
        big_cntr += 1
        dev.read(n_frames)
    if status.code == pyspectrum2.DeviceStatusCode.CONNECTED:
        dev.set_exposure(700)
        dev.read(n_frames)
    print(f'Got status: {status.code} {status.description}')


dev.set_frame_callback(on_frame)
dev.set_status_callback(on_status)
dev.run()


start_time = time()




while running:
    while len(frames) > 0:
        # print(cntr)
        data = frames.pop(-1)
        # print(data.shape)
        ax.clear()
        ax.plot(data)
        ax.text(1, 1, f'Batch {big_cntr}\nFrame {cntr}\nRunning for {int(time()-start_time)}s',horizontalalignment='right',
                transform=ax.transAxes,
                verticalalignment='top')
        frames.clear()
    plt.pause(0.05)

dev.stop()