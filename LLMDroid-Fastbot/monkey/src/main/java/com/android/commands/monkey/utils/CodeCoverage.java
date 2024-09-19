package com.android.commands.monkey.utils;

import android.os.Build;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CodeCoverage {

    // Static member variable, used to store log counts
    private static int methodCount = 0;

    private static int lastMethodCount = 1;

    private static double rate = 0.0;

    private static int total = 0;

    private final Map<String, Integer> summary = new HashMap<>();
    private final Set<String> visitedComponents = new HashSet<>();

    private static String logIdentifier;

    private static final String filePath = "/sdcard/codecoverage.txt";

    public CodeCoverage(int total_, String id) {
        try (FileWriter writer = new FileWriter(filePath, false);
             BufferedWriter bufferedWriter = new BufferedWriter(writer)) {
            bufferedWriter.write("code coverage");
            bufferedWriter.newLine();
        } catch (IOException e) {
            e.printStackTrace();
        }

        total = total_;
        logIdentifier = id;
        System.out.printf("[CodeCoverage] total methods: %d, TAG: %s\n", total, logIdentifier);
        // adb logcat -c
        try {
            Process process = Runtime.getRuntime().exec("logcat -c");
            System.out.println("[CodeCoverage] clear adb log cache");
        }
        catch (IOException e) {
            e.printStackTrace();
        }
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

    // Static method starts a new thread to listen to logcat output
    /*public void startLogcatListener() {
        new Thread(() -> {
            try {
                String cmd = "logcat -s " + logIdentifier;
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
    public void startLogcatListener() {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    try {
                        Runtime.getRuntime().exec("logcat -c");
                        System.out.println("[CodeCoverage] clear adb log cache");

                        String cmd = "logcat -s " + logIdentifier;
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
    }


    private static double computeIncrement() {
        int currentMethodCount;
        synchronized (CodeCoverage.class) {
            currentMethodCount = methodCount;
        }
        rate = ((double) (currentMethodCount - lastMethodCount) / lastMethodCount) * 100;
        double percentage = ((double) currentMethodCount / total) * 100;
        lastMethodCount = currentMethodCount;

        String str = String.format("[%s] %8.5f%% (%d/%d) --> %8.5f", logIdentifier ,percentage, currentMethodCount, total, rate);
        System.out.println(str);
        try (FileWriter writer = new FileWriter(filePath, true);
             BufferedWriter bufferedWriter = new BufferedWriter(writer)) {
            bufferedWriter.write(str);
            bufferedWriter.newLine();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return percentage;
    }

    // Static method to get the current count
    // actually return the current code coverage, not the growth rate
    public static double getGrowthRate() {
        return computeIncrement();
    }
}

