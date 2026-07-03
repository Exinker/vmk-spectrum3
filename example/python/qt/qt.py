import sys

import numpy as np

sys.path.append('cmake-build-debug/binding')
import matplotlib
matplotlib.use('Qt5Agg')
from ui import Ui_MainWindow
import pyspectrum3 as ps3

from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QWidget, QPushButton, QVBoxLayout, QHBoxLayout, QTextEdit, QGridLayout
from PyQt5.QtCore import QTimer

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from matplotlib.figure import Figure

class MplCanvas(FigureCanvasQTAgg):

    def __init__(self, parent=None, width=5, height=4, dpi=100):
        self.fig = Figure(figsize=(width, height), dpi=dpi)
        self.axes = self.fig.add_subplot(111)
        super(MplCanvas, self).__init__(self.fig)


class MainWindow(QtWidgets.QMainWindow):


    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)
        self.frame_queue = []
        self.status_queue = []

        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        new_plot_widget = MplCanvas(self)
        self.ui.gridLayout.replaceWidget(self.ui.plot_widget, new_plot_widget)
        self.ui.plot_widget = new_plot_widget
        # new_plot_widget.axes.plot([0,1,2,3,4], [10,1,20,3,40])

        self.device = ps3.DeviceManager()

        self.ui.connect_btn.clicked.connect(self.handle_connect)
        self.ui.xml_connect.clicked.connect(self.handle_xml_connect)
        self.ui.disconnect_btn.clicked.connect(self.handle_disconnect)
        self.ui.read_btn.clicked.connect(self.handle_read)
        self.ui.broadcastRead.clicked.connect(self.handle_broadcast_read)
        self.ui.stop_reading_btn.clicked.connect(self.handle_stop_reading)
        self.ui.set_exposure_btn.clicked.connect(self.handle_set_exposure)
        self.ui.checkBox.stateChanged.connect(self.handle_cr)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.on_timer)
        self.timer.start(32)

        self.on_disconnected()

        self.show()

    def log(self, s: str):
        self.ui.logs_widget.append(s)

    def frame_cb(self, data):
        if len(data) == 3:
            self.frame_queue.append(np.concatenate((data[0].result, data[1].result, data[2].result), axis=None))
        if len(data) == 2:
            self.frame_queue.append(np.concatenate((data[0].result, data[1].result), axis=None))
        else:
            self.frame_queue.append(data.result)

    def status_cb(self, statuses):
        self.log(f'[STATUS] {statuses}')
        status = ps3.AssemblyStatus.ALIVE
        for addr in statuses:
            if statuses[addr] != ps3.AssemblyStatus.ALIVE:
                status = statuses[addr]
                break
        self.status_queue.append(status)

    def handle_connect(self):
        self.log('Connecting...')
        cfg = ps3.AutoConfig().config()
        self.device.initialize(cfg)
        self.device.set_context_callback(self.frame_cb)
        self.device.set_status_callback(self.status_cb)
        self.device.set_assembly_filters([ps3.SwapFilter(), ps3.IntegratorFilter(100), ps3.NotifyFilter(100)])
        self.device.connect()
        self.device.set_exposure(100000)
        print(self.device.get_serial_numbers())
        # self.on_connected()

    def handle_xml_connect(self):
        self.log('Connecting...')
        cfg = ps3.ConfigFileParser("device_cfg.xml").read_file()
        self.device.initialize(cfg)
        self.device.set_frame_callback(self.frame_cb)
        self.device.set_status_callback(self.status_cb)
        self.device.connect()
        # self.on_connected()

    def handle_disconnect(self):
        self.log("Disconnecting...")
        self.device.disconnect()
        # self.on_disconnected()

    def handle_read(self):
        self.log('Starting read')
        try:
            #self.device.set_double_exposure(100000, 98, 3000000, 2)
            self.device.read(int(self.ui.frames_box.value()))
        except ValueError:
            self.log('Invalid number of frames')

    def handle_broadcast_read(self):
        self.log('Starting broadcast read')
        try:
            self.device.read_broadcast(int(self.ui.frames_box.value()))
        except ValueError:
            self.log('Invalid number of frames')

    def handle_stop_reading(self):
        self.log("Stopping reading")
        self.device.cancel_reading()

    def handle_set_exposure(self):
        self.log('Setting exposure')
        try:
            self.device.set_exposure(int(self.ui.exposure_box.value()))
        except ValueError:
            self.log('Invalid exposure value')

    def handle_cr(self, b):
        cr_value = 0 if b == 0 else 1
        self.log(f'Setting cr to {cr_value}')
        self.device.write_cr(0, cr_value, 0)



    def on_connected(self):
        self.ui.connect_btn.setEnabled(False)
        self.ui.xml_connect.setEnabled(False)
        self.ui.disconnect_btn.setEnabled(True)
        self.ui.read_btn.setEnabled(True)
        self.ui.broadcastRead.setEnabled(True)
        self.ui.stop_reading_btn.setEnabled(True)
        self.ui.set_exposure_btn.setEnabled(True)
        self.ui.checkBox.setEnabled(True)
        self.ui.statusbar.showMessage("Connected")

    def on_disconnected(self):
        self.ui.connect_btn.setEnabled(True)
        self.ui.xml_connect.setEnabled(True)
        self.ui.disconnect_btn.setEnabled(False)
        self.ui.read_btn.setEnabled(False)
        self.ui.broadcastRead.setEnabled(False)
        self.ui.stop_reading_btn.setEnabled(False)
        self.ui.set_exposure_btn.setEnabled(False)
        self.ui.checkBox.setEnabled(False)
        self.ui.checkBox.setChecked(False)
        self.ui.statusbar.showMessage("Disconnected")

    def on_reading(self):
        self.ui.connect_btn.setEnabled(False)
        self.ui.xml_connect.setEnabled(False)
        self.ui.disconnect_btn.setEnabled(False)
        self.ui.read_btn.setEnabled(False)
        self.ui.broadcastRead.setEnabled(False)
        self.ui.stop_reading_btn.setEnabled(True)
        self.ui.set_exposure_btn.setEnabled(False)
        self.ui.checkBox.setEnabled(False)
        self.ui.statusbar.showMessage("Reading")



    def on_timer(self):
        statuses = self.status_queue.copy()
        self.status_queue.clear()
        for status in statuses:
            if status == ps3.AssemblyStatus.ALIVE:
                self.on_connected()
            if status == ps3.AssemblyStatus.BYSY:
                self.on_reading()
            if status == ps3.AssemblyStatus.DISCONNECTED:
                self.on_disconnected()
        if len(self.frame_queue) == 0:
            return
        last_frame = self.frame_queue[0]
        self.frame_queue.clear()
        self.ui.plot_widget.axes.clear()
        self.ui.plot_widget.axes.plot(last_frame)
        self.ui.plot_widget.figure.canvas.draw()




app = QtWidgets.QApplication(sys.argv)
w = MainWindow()
app.exec_()