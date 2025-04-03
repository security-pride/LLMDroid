package com.android.commands.monkey.utils;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.jacoco.core.analysis.*;
import org.jacoco.core.tools.ExecFileLoader;
import org.jacoco.core.data.ExecutionDataStore;


public class CodeCoverage {

    private static final String filePath = "/sdcard/codecoverage.txt";

    // AndroLog
    private static int mTotal = 0;
    private static String mLogIdentifier;
    private static AndrologBridge mAndrologBridge = null;

    // Jacoco
    private static String mOutputDir = "";
    private static String mEcFileName = "";
    private static String mEcFilePath = "";
    private static String mClassFilePath = "";
    private static double mLastCoverage = 0.00001;
    private static JacocoBridge mJacocoBridge = null;

    // Constructor for AndroLog
    public CodeCoverage(int total_, String id) {
        saveToFile("code coverage", false);
        mTotal = total_;
        mLogIdentifier = id;
        Logger.format("[AndroLog] total methods: %d, TAG: %s\n", mTotal, mLogIdentifier);
        
        // adb logcat -c
        try {
            Process process = Runtime.getRuntime().exec("logcat -c");
            Logger.println("[AndroLog] clear adb log cache");
        }
        catch (IOException e) {
            e.printStackTrace();
        }

        mAndrologBridge = new AndrologBridge();
        mAndrologBridge.startLogcatListener();
    }

    // Constructor for Jacoco
    public CodeCoverage(File outputDir, String ecFileName, String ecFilePath, String classFilePath) {
        saveToFile("code coverage", false);
        mOutputDir = outputDir.getAbsolutePath();
        mEcFileName = ecFileName;
        mEcFilePath = ecFilePath;
        mClassFilePath = classFilePath;
        mJacocoBridge = new JacocoBridge(mOutputDir);
        System.out.println("mOutputDir: " + mOutputDir);
        System.out.println("mEcFileName: " + mEcFileName);
        System.out.println("mEcFilePath: " + mEcFilePath);
        System.out.println("mClassFilePath: " + mClassFilePath);
    }

    private static void saveToFile(String content, boolean append) {
        try (FileWriter writer = new FileWriter(filePath, append);
             BufferedWriter bufferedWriter = new BufferedWriter(writer)) {
            bufferedWriter.write(content);
            bufferedWriter.newLine();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    // Static method starts a new thread to listen to logcat output
    /*public void startLogcatListener() {
        new Thread(() -> {
            try {
                String cmd = "logcat -s " + mLogIdentifier;
                Process process = Runtime.getRuntime().exec(cmd);
                BufferedReader bufferedReader = new BufferedReader(
                        new InputStreamReader(process.getInputStream()));

                String line;
                while ((line = bufferedReader.readLine()) != null) {
                    analyzeLine(line);
                }
                System.out.println("[CodeCoverage] ******************** !! thread stop !! *************************");
            } catch (IOException e) {
                e.printStackTrace();
            }
        }).start();
    }*/
    

    // Static method to get the current count
    // actually return the current code coverage, not the growth rate
    public static double getCoverage() {
        if (mJacocoBridge != null) {
            return getCoverageUsingJacoco();
        }
        
        if (mAndrologBridge != null) {
            return getCoverageUsingAndrolog();
        }

        Logger.println("Error when getting Code Coverage");
        return 0.00001;
    }

    public static double getCoverageUsingAndrolog() {
        return mAndrologBridge.computeIncrement();
    }

    public static double getCoverageUsingJacoco() {
        final CountDownLatch latch = new CountDownLatch(1);  // 用于同步

        // 使用 Runnable 和 Thread 初始化来执行计算覆盖率的任务
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Logger.println("Preparing to call getMethodCoverage method.");

                    // 调用 JacocoBridge 的 getMethodCoverage 方法
                    JacocoBridge jacocoBridge = new JacocoBridge(mOutputDir);
                    float result = jacocoBridge.getMethodCoverage();
                    result *= 100;  // 转换为百分比

                    synchronized (CodeCoverage.class) {
                        mLastCoverage = result;
                    }

                    Logger.println(String.format("Result from getMethodCoverage: %.5f%%", result));

                } catch (IOException ex) {
                    Logger.println(String.format("Caught an IOException from Java: %s", ex.getMessage()));
                } finally {
                    latch.countDown();  // 通知主线程更新覆盖率
                }
            }
        });

        thread.start();  // 启动线程

        // 等待最多 1 秒
        double coverage = 0.00001;
        try {
            if (!latch.await(1, TimeUnit.SECONDS)) {
                Logger.println("Coverage calculation took too long, returning previous coverage.");
                synchronized (CodeCoverage.class) {
                    coverage = mLastCoverage;
                }
            } else {
                Logger.println(String.format("New coverage: %.5f%%", mLastCoverage));
                synchronized (CodeCoverage.class) {
                    coverage = mLastCoverage;
                }
            }
        } catch (InterruptedException e) {
            Logger.println("Thread interrupted while waiting for coverage calculation.");
        }

        saveToFile(String.format("%.5f%%", coverage), true);
        return coverage;
    }

    private static class AndrologBridge {
        
        private int methodCount = 0;
        private int lastMethodCount = 1;
        private double rate = 0.0;

        private final Map<String, Integer> summary = new HashMap<>();
        private final Set<String> visitedComponents = new HashSet<>();

        public void startLogcatListener() {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    while (true) {
                        try {
                            Runtime.getRuntime().exec("logcat -c");
                            System.out.println("[CodeCoverage] clear adb log cache");
    
                            String cmd = "logcat -s " + mLogIdentifier;
                            Process process = Runtime.getRuntime().exec(cmd);
                            BufferedReader bufferedReader = new BufferedReader(
                                    new InputStreamReader(process.getInputStream()));
    
                            String line;
                            while ((line = bufferedReader.readLine()) != null) {
                                analyzeLine(line);
                            }
                            System.out.println("[CodeCoverage] ******************** !! thread stop !! *************************");
                            System.out.println("[CodeCoverage] ******************** !! restart logcat !! *************************");
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
    
                }
            });
            thread.start();
            System.out.println("[Androlog] Logcat Listener started");
        }

        /**
         * Increments the count for a given key in the summary map.
         *
         * @param key The key whose count is to be incremented.
         */
        private void increment(String key) {
    //        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
    //            summary.merge(key, 1, Integer::sum);
    //        }
            synchronized (CodeCoverage.class) {
                methodCount++; // update count
            }
        }
            
        /**
         * Increments the count of logged methods.
         *
         * @param method The method to be logged.
         */
        private void incrementMethod(String method) {
            incrementComponent("methods", method);
        }
    
        /**
         * Increments the count for a specific component type and component name.
         * Ensures that each unique component is only counted once.
         *
         * @param type      The type of component (e.g., "methods", "classes").
         * @param component The name of the component.
         */
        private void incrementComponent(String type, String component) {
            if (visitedComponents.add(type + component)) {
                increment(type);
            }
        }
    
        private String extractContent(String line, String prefix) {
            Pattern pattern = Pattern.compile("=(.*)");
            Matcher matcher = pattern.matcher(line);
            if (matcher.find()) {
                return matcher.group(1);
            }
            return null;
        }
    
        private String getLogType(String line) {
            Pattern pattern = Pattern.compile("(\\w+?)=");
            Matcher matcher = pattern.matcher(line);
            if (matcher.find()) {
                return matcher.group(1);
            }
            return null;
        }
    
        private void analyzeLine(String line) {
            String logType = getLogType(line);
    
            if (Objects.equals(logType, "METHOD")) {
                incrementMethod(extractContent(line, "METHOD="));
            }
        }
    
        public void printSummary() {
            for (Map.Entry<String, Integer> entry : summary.entrySet()) {
                System.out.println(entry.getKey() + " : " + entry.getValue());
            }
        }
    
        public Map<String, Integer> getSummary() {
            return summary;
        }
    
        public Integer getMethodSummary() {
            return summary.get("methods");
        }

        public double computeIncrement() {
            int currentMethodCount;
            synchronized (CodeCoverage.class) {
                currentMethodCount = methodCount;
            }
            rate = ((double) (currentMethodCount - lastMethodCount) / lastMethodCount) * 100;
            double percentage = ((double) currentMethodCount / mTotal) * 100;
            lastMethodCount = currentMethodCount;
    
            String str = String.format("[%s] %8.5f%% (%d/%d) --> %8.5f", mLogIdentifier ,percentage, currentMethodCount, mTotal, rate);
            System.out.println(str);
            saveToFile(str, true);
            return percentage;
        }
        
    }
    
    private static class JacocoBridge implements ICoverageVisitor {

        private int sumCoveredCount = 0;
        private int sumMissedCount = 0;
        private int sumTotalCount = 0;

        private final String adbPullPath;    // The store path after pulling ec file, must be an absolute path!

        public JacocoBridge(String pullPath) {
            adbPullPath = (!pullPath.isEmpty()) ? pullPath: ".";
        }

        private void sendBroadcast() {
            try {// adb shell am broadcast -a com.llmdroid.jacoco.COLLECT_COVERAGE --es coverageFile "coverage1.ec"
                String command = String.format("am broadcast -a com.llmdroid.jacoco.COLLECT_COVERAGE --es coverageFile %s", mEcFileName);
                Process process = Runtime.getRuntime().exec(command);
                BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
                String line;
                while ((line = bufferedReader.readLine()) != null) {
                    System.out.println(line);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
            // sleep(0.3s)
        }

        public float getMethodCoverage() throws IOException {
            sendBroadcast();

            String targetPath = mEcFilePath + "/" + mEcFileName;
            adbPullFile(targetPath);

            // ec file to ExecutionDataStore
            System.out.println("Load ec file");
            ExecFileLoader execFileLoader = new ExecFileLoader();
            File ecFile = new File(adbPullPath + "/" + mEcFileName);
            execFileLoader.load(ecFile);
            ExecutionDataStore store = execFileLoader.getExecutionDataStore();

            // analyzer
            System.out.println("Begin analyzing...");
            Analyzer analyzer = new Analyzer(store, this);
            analyzer.analyzeAll(new File(mClassFilePath));

            // print result
            Logger.format("[Jacoco]: covered: %d; missed: %d, total: %d\n",
                    sumCoveredCount, sumMissedCount, sumTotalCount);

            return (float)sumCoveredCount / sumTotalCount;
        }

        private void adbPullFile(String source) {

            String command = String.format("cp %s %s/", source, adbPullPath);

            try {
                Process process = Runtime.getRuntime().exec(command);
                BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println(line);
                }
                int exitCode = process.waitFor();
                if (exitCode == 0) {
                    System.out.println("File copied successfully.");
                } else {
                    System.out.println("Failed to copy file, command: " + command);
                }
            } catch (IOException | InterruptedException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void visitCoverage(IClassCoverage iClassCoverage) {
            ICounter methodCounter = iClassCoverage.getMethodCounter();
    //        ICounter lineCounter = iClassCoverage.getLineCounter();
            int coveredCount = methodCounter.getCoveredCount();
            int missedCount = methodCounter.getMissedCount();
            int totalCount = methodCounter.getTotalCount();

            sumCoveredCount += coveredCount;
            sumMissedCount += missedCount;
            sumTotalCount += totalCount;
    //        System.out.printf("[%s]: covered: %d; missed: %d, total: %d\n", iClassCoverage.getName(),
    //                lineCounter.getCoveredCount(), lineCounter.getMissedCount(), lineCounter.getTotalCount());
        }

    }
}

