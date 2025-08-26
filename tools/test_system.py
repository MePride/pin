#!/usr/bin/env python3
"""
Pin Device System Test Script
系统集成测试工具
"""

import requests
import json
import time
import argparse
import sys
from typing import Dict, Any, Optional

class PinDeviceTest:
    def __init__(self, device_ip: str = "192.168.4.1", timeout: int = 10):
        self.device_ip = device_ip
        self.base_url = f"http://{device_ip}"
        self.timeout = timeout
        self.session = requests.Session()
        self.test_results = []
        
    def log_test(self, test_name: str, success: bool, message: str = ""):
        """记录测试结果"""
        result = {
            "test": test_name,
            "success": success,
            "message": message,
            "timestamp": time.time()
        }
        self.test_results.append(result)
        
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{status} {test_name}: {message}")
        
    def test_device_connectivity(self) -> bool:
        """测试设备连通性"""
        try:
            response = self.session.get(f"{self.base_url}/", timeout=self.timeout)
            if response.status_code == 200:
                self.log_test("Device Connectivity", True, f"Device responds on {self.device_ip}")
                return True
            else:
                self.log_test("Device Connectivity", False, f"HTTP {response.status_code}")
                return False
        except requests.exceptions.RequestException as e:
            self.log_test("Device Connectivity", False, str(e))
            return False
    
    def test_pwa_manifest(self) -> bool:
        """测试PWA清单文件"""
        try:
            response = self.session.get(f"{self.base_url}/manifest.json", timeout=self.timeout)
            if response.status_code == 200:
                manifest = response.json()
                required_fields = ["name", "short_name", "start_url", "display", "icons"]
                
                for field in required_fields:
                    if field not in manifest:
                        self.log_test("PWA Manifest", False, f"Missing required field: {field}")
                        return False
                
                self.log_test("PWA Manifest", True, "Valid PWA manifest")
                return True
            else:
                self.log_test("PWA Manifest", False, f"HTTP {response.status_code}")
                return False
        except (requests.exceptions.RequestException, json.JSONDecodeError) as e:
            self.log_test("PWA Manifest", False, str(e))
            return False
    
    def test_api_status(self) -> bool:
        """测试设备状态API"""
        try:
            response = self.session.get(f"{self.base_url}/api/status", timeout=self.timeout)
            if response.status_code == 200:
                status = response.json()
                required_fields = ["device", "battery", "wifi"]
                
                for field in required_fields:
                    if field not in status:
                        self.log_test("API Status", False, f"Missing field: {field}")
                        return False
                
                device_info = status.get("device", {})
                battery_info = status.get("battery", {})
                wifi_info = status.get("wifi", {})
                
                # 验证设备信息
                if "name" not in device_info or "version" not in device_info:
                    self.log_test("API Status", False, "Invalid device info")
                    return False
                
                # 验证电池信息
                if "percentage" not in battery_info or "voltage" not in battery_info:
                    self.log_test("API Status", False, "Invalid battery info")
                    return False
                
                self.log_test("API Status", True, 
                             f"Device: {device_info.get('name')} v{device_info.get('version')}, "
                             f"Battery: {battery_info.get('percentage')}%")
                return True
            else:
                self.log_test("API Status", False, f"HTTP {response.status_code}")
                return False
        except (requests.exceptions.RequestException, json.JSONDecodeError) as e:
            self.log_test("API Status", False, str(e))
            return False
    
    def test_wifi_scan(self) -> bool:
        """测试WiFi扫描API"""
        try:
            response = self.session.get(f"{self.base_url}/api/wifi/scan", timeout=30)  # WiFi扫描需要更长时间
            if response.status_code == 200:
                scan_result = response.json()
                
                if "networks" not in scan_result:
                    self.log_test("WiFi Scan", False, "Invalid scan response format")
                    return False
                
                networks = scan_result["networks"]
                if not isinstance(networks, list):
                    self.log_test("WiFi Scan", False, "Networks should be a list")
                    return False
                
                self.log_test("WiFi Scan", True, f"Found {len(networks)} networks")
                
                # 验证网络信息格式
                for network in networks[:3]:  # 只检查前3个网络
                    required_fields = ["ssid", "rssi", "auth"]
                    for field in required_fields:
                        if field not in network:
                            self.log_test("WiFi Scan", False, f"Network missing field: {field}")
                            return False
                
                return True
            else:
                self.log_test("WiFi Scan", False, f"HTTP {response.status_code}")
                return False
        except (requests.exceptions.RequestException, json.JSONDecodeError) as e:
            self.log_test("WiFi Scan", False, str(e))
            return False
    
    def test_plugin_list(self) -> bool:
        """测试插件列表API"""
        try:
            response = self.session.get(f"{self.base_url}/api/plugins", timeout=self.timeout)
            if response.status_code == 200:
                plugins = response.json()
                
                if not isinstance(plugins, list):
                    self.log_test("Plugin List", False, "Plugins should be a list")
                    return False
                
                # 验证插件信息格式
                for plugin in plugins:
                    required_fields = ["name", "description", "enabled"]
                    for field in required_fields:
                        if field not in plugin:
                            self.log_test("Plugin List", False, f"Plugin missing field: {field}")
                            return False
                
                self.log_test("Plugin List", True, f"Found {len(plugins)} plugins")
                
                # 检查是否有时钟插件
                clock_plugin = next((p for p in plugins if p["name"] == "clock"), None)
                if clock_plugin:
                    self.log_test("Clock Plugin", True, f"Clock plugin found, enabled: {clock_plugin['enabled']}")
                else:
                    self.log_test("Clock Plugin", False, "Clock plugin not found")
                
                return True
            else:
                self.log_test("Plugin List", False, f"HTTP {response.status_code}")
                return False
        except (requests.exceptions.RequestException, json.JSONDecodeError) as e:
            self.log_test("Plugin List", False, str(e))
            return False
    
    def test_settings_api(self) -> bool:
        """测试设置API"""
        try:
            # 先获取当前设置
            response = self.session.get(f"{self.base_url}/api/settings", timeout=self.timeout)
            if response.status_code == 200:
                settings = response.json()
                self.log_test("Settings Get", True, "Settings retrieved successfully")
                
                # 测试设置更新
                test_settings = {
                    "refresh_interval": 60,
                    "power_save_mode": True,
                    "auto_brightness": False
                }
                
                update_response = self.session.post(
                    f"{self.base_url}/api/settings",
                    json=test_settings,
                    timeout=self.timeout
                )
                
                if update_response.status_code == 200:
                    self.log_test("Settings Update", True, "Settings updated successfully")
                    return True
                else:
                    self.log_test("Settings Update", False, f"HTTP {update_response.status_code}")
                    return False
            else:
                self.log_test("Settings Get", False, f"HTTP {response.status_code}")
                return False
        except (requests.exceptions.RequestException, json.JSONDecodeError) as e:
            self.log_test("Settings API", False, str(e))
            return False
    
    def run_all_tests(self) -> bool:
        """运行所有测试"""
        print(f"🚀 Starting Pin Device System Tests")
        print(f"🎯 Target Device: {self.device_ip}")
        print("=" * 50)
        
        tests = [
            self.test_device_connectivity,
            self.test_pwa_manifest,
            self.test_api_status,
            self.test_wifi_scan,
            self.test_plugin_list,
            self.test_settings_api
        ]
        
        passed = 0
        total = len(tests)
        
        for test in tests:
            try:
                if test():
                    passed += 1
                time.sleep(1)  # 测试之间的间隔
            except Exception as e:
                print(f"❌ Test failed with exception: {e}")
        
        print("=" * 50)
        print(f"📊 Test Results: {passed}/{total} tests passed")
        
        if passed == total:
            print("🎉 All tests passed! System integration successful.")
            return True
        else:
            print("⚠️  Some tests failed. Please check the system.")
            return False
    
    def save_results(self, filename: str):
        """保存测试结果到文件"""
        with open(filename, 'w') as f:
            json.dump(self.test_results, f, indent=2)
        print(f"📁 Test results saved to {filename}")

def main():
    parser = argparse.ArgumentParser(description="Pin Device System Test")
    parser.add_argument("--ip", default="192.168.4.1", help="Device IP address")
    parser.add_argument("--timeout", type=int, default=10, help="Request timeout in seconds")
    parser.add_argument("--save", help="Save results to JSON file")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    
    args = parser.parse_args()
    
    if args.verbose:
        print(f"Configuration:")
        print(f"  Device IP: {args.ip}")
        print(f"  Timeout: {args.timeout}s")
        print(f"  Save results: {args.save or 'No'}")
        print()
    
    tester = PinDeviceTest(args.ip, args.timeout)
    success = tester.run_all_tests()
    
    if args.save:
        tester.save_results(args.save)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()