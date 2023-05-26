"""
1. Auto Login
    1.1 Use selenium to grab tickets.
    1.2 Determine if you are currently logged in.
        1.2.1 if cookies.pkl no need
        1.2.2 else cookies.pkl need
2. Grab Tickets
"""

from selenium import webdriver
import os
import time

# damai link
damai_url = 'https://m.damai.cn/damai/home/index.html'
# login
login_url = 'https://m.damai.cn/damai/minilogin/index.html?returnUrl=https%3A%2F%2Fm.damai.cn%2Fdamai%2Fmine%2Fmy%2Findex.html%3Fspm%3Da2o71.home.top.duserinfo&spm=a2o71.mydamai.0.0'
#target_url_login = 'https://m.damai.cn/damai/minilogin/index.html?returnUrl=https%3A%2F%2Fm.damai.cn%2Fapp%2Fdmfe%2Fselect-seat-biz%2Fkylin.html%3FitemId%3D716320736896%26performId%3D211216271%26skuId%3D5174636738645%26projectId%3D215196035%26toDxOrder%3Dtrue%26quickBuy%3D0%26channel%3Ddamai_app%26spm%3Dundefined%26userPromotion%3Dfalse&spm=a2o71.project.0.0'
# Grab Tickets link
target_url = 'https://m.damai.cn/damai/detail/item.html?itemId=716320736896&spm=a2o71.category.itemlist.ditem_0'

class Concert:
    """ 初始化加载 """
    def __init__(self):
        self.status = 0                   # 状态，当前操作执行到哪一步
        self.login_method = 1             # 0:手动登录, 1:cookie登录
        self.driver = webdriver.Chrome()  # 初始化浏览器驱动
    
    """ 获取记录用户信息的cookie """
    def set_cookies(self):
        self.driver.get(login_url)
        print("###请手动扫码登录###")
        time.sleep(10)
        print("###扫码登录成功###")

    """ 读取cookie """
    def get_cookies(self):
        # 读取cookie
        cookies = pickle.load(open("cookies.pkl", "rb"))
        for cookie in cookies:
            self.driver.add_cookie(cookie)
        # 再次访问页面，便可实现免登陆访问
        self.driver.get(target_url)

    """ 登录 """
    def login(self):
        # 如果为0，先手动登陆一下
        if self.login_method == 0:
            self.driver.get(login_url)
        elif self.login_method == 1:
            # 如果当前目录没有cookie.pkl文件则先手动登录一次获取cookie，以后都可以用这个cookie登录了
            if not os.path.exists('cookies.pkl'):
                self.set_cookies()
            else:
                self.driver.get(target_url)
                # cookike登录 通过seleium传入用户信息
                self.get_cookies()

    def __str__(self):
        return self.name + ' ' + self.time + ' ' + self.place + ' ' + self.price

    def __repr__(self):
        return self.name + ' ' + self.time + ' ' + self.place + ' ' + self.price





